#include "logging.hpp"
#include "searcher.hpp"

#include <algorithm>
#include <cstdint>
#include <fcntl.h>
#include <numeric>
#include <vector>

Searcher::Searcher(const std::string& ip, uint16_t port, uint32_t searchInterval, const std::shared_ptr<Protocol>& protocol, PvFoundCb& foundPvCb)
    : m_searchInterval(searchInterval)
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

        LOG_DEBUG("Received UDP packet (", recvd, " bytes) from ", iocIp, ":", iocPort, ", potential PV(s) search response");

        auto responses = m_protocol->parseSearchResponse({buffer, buffer+recvd});
        for (auto& [chanId, rsp]: responses) {
            // IOC might have returned 255.255.255.255 in the CA reply for the client
            // to use the IP address from the socket. But this doesn't work when
            // nameserver is in between, so we need to set the IOC's IP in the packet.
            m_protocol->updateSearchReply(rsp, iocIp, iocPort);

            // PVs searched last are the end of the list. Iterate backwards from the end
            // because that's most likely to find the PV quicker
            for (auto it = m_searchedPvs.rbegin(); it != m_searchedPvs.rend(); it++) {
                if (it->chanId == chanId) {
                    auto pvname = it->pvname;

                    m_searchedPvs.erase(std::next(it).base());

                    LOG_VERBOSE("Found ", pvname, " on ", iocIp, ":", iocPort);
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
    std::list<SearchedPV> retries;

    // Stop at the first PV of which the search time is in the future
    while (m_searchedPvs.empty() == false && m_searchedPvs.front().nextSearch < now) {
        auto& pv = m_searchedPvs.front();

        // Add to the list of PVs to be searched for this time
        pvs.emplace_back(pv.chanId, pv.pvname);

        // Unlike EPICS base, use a static search interval after first 3 tries
        if (++pv.retries > 3) {
            // This will move PV to the end of the m_searchInterval list
            scheduleNextSearch(pv.pvname, m_searchInterval);
        } else {
            // We can't insert the entry back to the front or we would loop forever
            // so we move it to a temporary list
            retries.splice(retries.end(), m_searchedPvs, m_searchedPvs.begin());
        }
    }

    // Send some PVs in each iteration depending how large packets are allowed by a given protol.
    // Keep iterating until there's more PVs to search for
    while (pvs.empty() == false) {
        const auto [msg, nPvs] = m_protocol->createSearchRequest(pvs);

        std::string tmp;
        std::for_each(pvs.begin(), pvs.begin()+nPvs, [&tmp](auto& it) { tmp += it.second + ","; });
        tmp.pop_back();
        LOG_VERBOSE("Sending search request for ", tmp, " to ", m_searchIp, ":", m_searchPort);

        ::sendto(m_sock, msg.data(), msg.size(), 0, reinterpret_cast<sockaddr *>(&m_addr), sizeof(sockaddr_in));

        pvs.erase(pvs.begin(), pvs.begin() + nPvs);
    }

    // Put the PVs to be retried back to the beginning of the list
    m_searchedPvs.splice(m_searchedPvs.begin(), retries, retries.begin(), retries.end());
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

    // Cache the nextSearch timestamp to be used later during the iteration
    auto nextSearch = it->nextSearch;

    // Move the element to the end of the list, hopefully that's the final position
    m_searchedPvs.splice(m_searchedPvs.end(), m_searchedPvs, it);

    // Find insertion point from the back
    it = m_searchedPvs.end();
    while (it != m_searchedPvs.begin()) {
        --it;
        if (it->nextSearch <= nextSearch) {
            m_searchedPvs.splice(std::next(it), m_searchedPvs, std::prev(m_searchedPvs.end()));
            return;
        }
    }

    // If it's the smallest element, move it to the front
    m_searchedPvs.splice(m_searchedPvs.begin(), m_searchedPvs, std::prev(m_searchedPvs.end()));
}
