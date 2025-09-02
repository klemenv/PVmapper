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
}

uint32_t Searcher::getNextChanId()
{
    if (++m_chanId == INT32_MAX) {
        // Change ids of all searched PVs
        m_chanId = 0;
        for (auto& pv: m_searchedPvs) {
            pv.chanId = m_chanId++;
        }
    }
    return m_chanId;
}

bool Searcher::addPV(const std::string& pvname)
{
    for (auto& pv: m_searchedPvs) {
        if (pv.pvname == pvname) {
            // We're already searching for this PV
            pv.lastSearched = std::chrono::steady_clock::now();
            return false;
        }
    }

    // Prepend the PV to the front of the list to be picked up next time we search for PVs
    SearchedPV pv;
    pv.pvname = pvname;
    pv.nextSearch = std::chrono::steady_clock::now();
    pv.lastSearched = std::chrono::steady_clock::now();
    pv.chanId = m_chanId++;
    m_searchedPvs.emplace_front(pv);

    return true;
}

void Searcher::removePV(const std::string& pvname)
{
    for (auto it = m_searchedPvs.begin(); it != m_searchedPvs.end(); it++) {
        if (it->pvname == pvname) {
            m_searchedPvs.erase(it);
            return;
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

            // Most likely the PV exists all the time and the search reply
            // will come back when the search intervals are small.
            // In other words, those PVs will be at the beggining of the queue.
            for (auto it = m_searchedPvs.begin(); it != m_searchedPvs.end(); it++) {
                if (it->chanId == chanId) {
                    auto pvname = it->pvname;

                    m_searchedPvs.erase(it);

                    LOG_VERBOSE("Found ", pvname, " on ", DnsCache::resolveIP(iocIp), ":", iocPort);
                    m_foundPvCb(pvname, iocIp, iocPort, rsp);

                    break;
                }
            }
        }
        recvd = ::recvfrom(m_sock, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr *>(&remoteAddr), &remoteAddrLen);
    }
}

void Searcher::processOutgoing()
{
    auto now = std::chrono::steady_clock::now();
    std::vector<std::pair<uint32_t, std::string>> pvs;
    std::list<std::pair<std::string, uint32_t>> intervals;

    // Stop at the first PV of which the search time is in the future
    for (auto it = m_searchedPvs.begin(); it != m_searchedPvs.end() && it->nextSearch < now; it++) {
        it->retries++;

        // Add to the list of PVs to be searched for this time
        pvs.emplace_back(it->chanId, it->pvname);

        // Calculate next search interval
        uint32_t interval;
        if (it->retries < 3) {
            interval = 0;
        } else if ((it->retries-3) < m_searchIntervals.size()) {
            interval = m_searchIntervals[it->retries - 3];
        } else if (m_searchIntervals.empty() == false) {
            interval = m_searchIntervals.back();
        } else {
            interval = 300;
        }
        intervals.push_back(std::pair(it->pvname, interval));
    }

    // This is a separate loop because is shuffles the m_searchedPvs list
    for (auto& [pvname, interval]: intervals) {
        scheduleNextSearch(pvname, interval);
    }

    // Send some PVs in each iteration depending how large packets are allowed by a given protol.
    // Keep iterating until there's more PVs to search for
    while (pvs.empty() == false) {
        const auto [msg, nPvs] = m_protocol->createSearchRequest(pvs);

        std::string tmp;
        std::for_each(pvs.begin(), pvs.begin()+nPvs, [&tmp](auto& it) { tmp += it.second + ","; });
        tmp.pop_back();
        LOG_VERBOSE("Sending search request for ", tmp, " to ", DnsCache::resolveIP(m_searchIp), ":", m_searchPort);

        ::sendto(m_sock, msg.data(), msg.size(), 0, reinterpret_cast<sockaddr *>(&m_addr), sizeof(sockaddr_in));

        pvs.erase(pvs.begin(), pvs.begin() + nPvs);
    }
}

std::pair<uint32_t, uint32_t> Searcher::purgePVs(unsigned maxtime)
{
    unsigned nPurged = 0;
    for (auto it = m_searchedPvs.begin(); it != m_searchedPvs.end();) {
        auto diff = std::chrono::steady_clock::now() - it->lastSearched;
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(diff).count();
        if (duration > maxtime) {
            LOG_VERBOSE("Purged ", it->pvname, ", last searched ", duration, " seconds ago");
            it = m_searchedPvs.erase(it);
            nPurged++;
        } else {
            it++;
        }
    }

    return std::make_pair(nPurged, m_searchedPvs.size());
}

void Searcher::scheduleNextSearch(const std::string& pvname, uint32_t delay)
{
    // Find an iterator to the element in the list
    auto it = std::find_if(m_searchedPvs.begin(), m_searchedPvs.end(), [&pvname](auto &el) {
        return (el.pvname == pvname);
    });

    if (it == m_searchedPvs.end()) return;

    // Increment the timestamp
    it->nextSearch += std::chrono::seconds(delay);

    // Performance optimization: skip continously searching PVs
    // because they're always at the end of the queue
    if (it->nextSearch < m_searchedPvs.back().nextSearch) {

        // Find proper position in a sorted list
        for (auto jt = m_searchedPvs.begin(); jt != m_searchedPvs.end(); jt++) {
            if (it->nextSearch <= jt->nextSearch) {
                m_searchedPvs.splice(jt, m_searchedPvs, it);
                return;
            }
        }
    }

    m_searchedPvs.splice(m_searchedPvs.end(), m_searchedPvs, it);
}
