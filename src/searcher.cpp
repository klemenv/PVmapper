#include "dnscache.hpp"
#include "logging.hpp"
#include "searcher.hpp"

#include <algorithm>
#include <cstdint>
#include <fcntl.h>
#include <numeric>
#include <vector>

Searcher::Searcher(const std::string& ip, uint16_t port, const std::vector<uint32_t>& searchIntervals, const std::shared_ptr<Protocol>& protocol, PvFoundCb& foundPvCb)
    : m_searchIntervals(searchIntervals)
    , m_protocol(protocol)
    , m_foundPvCb(foundPvCb)
    , m_searchIp(ip)
    , m_searchPort(port)
{
    m_sock = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (m_sock < 0) {
        throw SocketException("failed to create socket - {errno}");
    }

    int enable = 1;
    if (::setsockopt(m_sock, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable)) < 0) {
        throw SocketException("failed to enable broadcast on socket - {errno}");
    }

    if (::fcntl(m_sock, F_SETFL, fcntl(m_sock, F_GETFL, 0) | O_NONBLOCK) == -1) {
        throw SocketException("failed to set socket non-blocking", errno);
    }

    m_addr = {}; // avoid using memset()
    m_addr.sin_family = AF_INET;
    m_addr.sin_port = ::htons(port);
    if (::inet_aton(ip.c_str(), reinterpret_cast<in_addr*>(&m_addr.sin_addr.s_addr)) == 0) {
        throw SocketException("invalid IP address - {errno}");
    }

    // Search at most every 0.1s
    for (auto& interval: m_searchIntervals) {
        interval *= 10;
    }

    // Start every search with 3 packets, one right now and two in next two attempts
    m_searchIntervals.insert(m_searchIntervals.begin(), 2);
    m_searchIntervals.insert(m_searchIntervals.begin(), 1);

    // Now allocate buckets of PVs to be searched for every 0.1s apart
    auto maxInterval = m_searchIntervals.back();
    auto nBins = maxInterval;
    m_searchedPvs.resize(nBins);

    m_lastSearch = std::chrono::steady_clock::now();
}

uint32_t Searcher::getNextChanId()
{
    if (++m_chanId == INT32_MAX) {
        // Change ids of all searched PVs
        m_chanId = 0;
        for (auto& bin: m_searchedPvs) {
            for (auto& pv: bin) {
                pv.chanId = m_chanId++;
            }
        }
    }
    return m_chanId;
}

bool Searcher::addPV(const std::string& pvname)
{
    for (auto& bin: m_searchedPvs) {
        for (auto& pv: bin) {
            if (pv.pvname == pvname) {
                // We're already searching for this PV
                pv.lastSearched = std::chrono::steady_clock::now();
                return false;
            }
        }
    }

    // Prepend the PV to the first bucket to be picked up next time we search for PVs
    SearchedPV pv;
    pv.pvname = pvname;
    pv.lastSearched = std::chrono::steady_clock::now();
    pv.chanId = m_chanId++;
    pv.intervals = m_searchIntervals;
    m_searchedPvs[m_currentBin].emplace_front(pv);

    return true;
}

void Searcher::removePV(const std::string& pvname)
{
    for (auto& bin: m_searchedPvs) {
        for (auto jt = bin.begin(); jt != bin.end(); jt++) {
            if (jt->pvname == pvname) {
                bin.erase(jt);
                return;
            }
        }
    }
}

void Searcher::processIncoming()
{
    char buffer[4096];
    struct sockaddr_in remoteAddr;
    socklen_t remoteAddrLen = sizeof(remoteAddr);
    auto recvd = ::recvfrom(m_sock, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr *>(&remoteAddr), &remoteAddrLen);
    while (recvd > 0) {
        char iocIp[20] = {0};
        ::inet_ntop(AF_INET, &remoteAddr.sin_addr, iocIp, sizeof(iocIp)-1);
        uint16_t udpPort = ::ntohs(remoteAddr.sin_port);

        // Decode TCP port from the response packet
        auto [_, iocPort] = m_protocol->parseIocAddr(iocIp, udpPort, {buffer, buffer+recvd});

        LOG_DEBUG("Received UDP packet (", recvd, " bytes) from ", DnsCache::resolveIP(iocIp), ":", iocPort, ", potential PV(s) search response");

        auto responses = m_protocol->parseSearchResponse({buffer, buffer+recvd});
        for (auto& [chanId, rsp]: responses) {
            // IOC might have returned 255.255.255.255 in the CA reply for the client
            // to use the IP address from the socket. But this doesn't work when
            // nameserver is in between, so we need to set the IOC's IP in the packet.
            m_protocol->updateSearchReply(rsp, iocIp, iocPort);

            // The PV must be in one of the bins
            for (size_t i = 0; i < m_searchedPvs.size(); i++) {
                // Most likely the PV exists all the time and we'll find the PV right away.
                // Look in the most recent bin first.
                auto& bin = m_searchedPvs[(m_currentBin+i-1)%m_searchedPvs.size()];
                for (auto it = bin.begin(); it != bin.end(); it++) {
                    if (it->chanId == chanId) {
                        auto pvname = it->pvname;

                        bin.erase(it);

                        LOG_VERBOSE("Found ", pvname, " on ", DnsCache::resolveIP(iocIp), ":", iocPort);
                        m_foundPvCb(pvname, iocIp, iocPort, rsp);

                        i = m_searchedPvs.size();
                        break;
                    }
                }
            }
        }
        recvd = ::recvfrom(m_sock, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr *>(&remoteAddr), &remoteAddrLen);
    }
}

void Searcher::processOutgoing()
{
    // Enforce 10Hz processing, but leave just a bit of tolerance
    auto diff = std::chrono::steady_clock::now() - m_lastSearch;
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();
    if (duration < 99) {
        return;
    }

    std::vector<std::pair<uint32_t, std::string>> pvs;

    auto& bin = m_searchedPvs[m_currentBin];

    for (auto it = bin.begin(); it != bin.end();) {
        // Add to the list of PVs to be searched for this time
        pvs.emplace_back(it->chanId, it->pvname);

        // If not the last search interval, determine the new bin
        // and move the element to that queue. If it is the last
        // search interval, we leave it in current queue since it's
        // exactly the max interval apart from now.
        if (it->intervals.size() > 1) {
            auto newBinIdx = (m_currentBin + it->intervals.front()) % m_searchedPvs.size();
            it->intervals.erase(it->intervals.begin());
            auto jt = it++; // the next command will invalidate current it iterator, make a copy
            m_searchedPvs[newBinIdx].splice(m_searchedPvs[newBinIdx].begin(), bin, jt);
        } else {
            it++;
        }
    }
    m_currentBin = ((m_currentBin + 1) % m_searchedPvs.size());

    // Send some PVs in each iteration depending how large packets are allowed by a given protol.
    // Keep iterating until there's more PVs to search for
    while (pvs.empty() == false) {
        const auto [msg, nPvs] = m_protocol->createSearchRequest(pvs);

        std::string tmp;
        std::for_each(pvs.begin(), pvs.begin()+nPvs, [&tmp](auto& jt) { tmp += jt.second + ","; });
        tmp.pop_back();
        LOG_VERBOSE("Sending search request for ", tmp, " to ", DnsCache::resolveIP(m_searchIp), ":", m_searchPort);

        ::sendto(m_sock, msg.data(), msg.size(), 0, reinterpret_cast<sockaddr *>(&m_addr), sizeof(sockaddr_in));

        pvs.erase(pvs.begin(), pvs.begin() + nPvs);
    }
}

std::pair<uint32_t, uint32_t> Searcher::purgePVs(unsigned maxtime)
{
    unsigned nPurged = 0;
    std::list<SearchedPV> pvs;
    for (auto& bin: m_searchedPvs) {
        for (auto it = bin.begin(); it != bin.end();) {
            auto diff = std::chrono::steady_clock::now() - it->lastSearched;
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(diff).count();
            if (duration > maxtime) {
                LOG_VERBOSE("Purged ", it->pvname, ", last searched ", duration, " seconds ago");
                it = bin.erase(it);
                nPurged++;
            } else {
                it++;
            }
        }

        // Move all PVs to a temporary queue where they can be balanced to bins evenly
        pvs.splice(pvs.end(), bin);
        bin.clear();
    }
    auto nSearching = pvs.size();

    // Balance the PVs in bins evenly, allowing some bins to be empty if the total number of PVs is small
    size_t pvsPerBin = std::ceil(1.0 * pvs.size() / m_searchedPvs.size());
    for (size_t i = 0; i < m_searchedPvs.size() && !pvs.empty(); i++) {
        auto& bin = m_searchedPvs[i];
        size_t nPvs = std::min(pvsPerBin, pvs.size());
        if (nPvs < 10) {
            // Not optimal to send only a few PVs in a UDP packet, let's 
            // combine some PVs. Pick 10 as conservative number of how many
            // PVs can fit in a single packet, but still significant improvement
            // when pvsPerBin is very small.
            i += (10 - nPvs - 1);
            nPvs = std::min(pvs.size(), (size_t)10);
        }
        bin.splice(bin.begin(), pvs, pvs.begin(), std::next(pvs.begin(), nPvs));
    }

    // Unlikely but possible scenario: if any PVs remain (due to rounding or skip logic),
    // add them to the last bin
    if (!pvs.empty()) {
        m_searchedPvs.back().splice(m_searchedPvs.back().end(), pvs);
    }

    m_currentBin = 0;

    return std::make_pair(nPurged, nSearching);
}
