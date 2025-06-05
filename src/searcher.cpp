#include "logging.hpp"
#include "searcher.hpp"

#include <cstdint>
#include <fcntl.h>
#include <numeric>

Searcher::Searcher(const std::string& ip, uint16_t port, const std::shared_ptr<AbstractProtocol>& protocol, PvFoundCb& foundPvCb)
    : m_protocol(protocol)
    , m_foundPvCb(foundPvCb)
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

void Searcher::addPV(const std::string& pvname)
{
    for (auto& pv: m_searchedPvs) {
        if (pv.pvname == pvname) {
            // We're already searching for this PV
            return;
        }
    }

    SearchedPV pv;
    pv.pvname = pvname;
    pv.nextSearch = std::chrono::steady_clock::now();
    pv.chanId = m_chanId++;
    m_searchedPvs.emplace_back(pv);

    LOG_DEBUG("Started searching for ", pvname);
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

            // PVs searched last at are the end of the list. Iterate backwards from the end
            // because that's most likely to find the PV quicker
            for (auto it = m_searchedPvs.rbegin(); it != m_searchedPvs.rend(); it++) {
                if (it->chanId == chanId) {
                    auto pvname = it->pvname;

                    LOG_VERBOSE("Found ", pvname, " on ", iocIp, ":", iocPort);
                    m_foundPvCb(pvname, iocIp, iocPort, rsp);

                    m_searchedPvs.erase(std::next(it).base());
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

        pvs.emplace_back(pv.chanId, pv.pvname);

        // Unlike EPICS base, use a static search interval after first 3 tries
        if (++pv.retries > 3) {
            pv.nextSearch = std::chrono::steady_clock::now() + std::chrono::seconds(m_searchInterval);

            m_searchedPvs.splice(m_searchedPvs.end(), m_searchedPvs, m_searchedPvs.begin());
        } else {
            retries.splice(retries.end(), m_searchedPvs, m_searchedPvs.begin());
        }

        // Send up to 10 PVs in one go
        if (pvs.size() == 10) {
            std::string tmp;
            std::for_each(pvs.begin(), pvs.end(), [&tmp](auto& it) { tmp += it.second + ","; });
            tmp.pop_back();
            LOG_VERBOSE("Sending search request for ", tmp);

            auto msg = m_protocol->createSearchRequest(pvs);
            ::sendto(m_sock, msg.data(), msg.size(), 0, reinterpret_cast<sockaddr *>(&m_addr), sizeof(sockaddr_in));
            pvs.clear();
        }
    }

    if (pvs.size() > 0) {
        std::string tmp;
        std::for_each(pvs.begin(), pvs.end(), [&tmp](auto& it) { tmp += it.second + ","; });
        tmp.pop_back();
        LOG_VERBOSE("Sending search request for ", tmp);

        auto msg = m_protocol->createSearchRequest(pvs);
        ::sendto(m_sock, msg.data(), msg.size(), 0, reinterpret_cast<sockaddr *>(&m_addr), sizeof(sockaddr_in));
    }

    // Put the PVs to be retried back to the beginning of the list
    m_searchedPvs.splice(m_searchedPvs.begin(), retries, retries.begin(), retries.end());
}
