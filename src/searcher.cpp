#include "logging.hpp"
#include "searcher.hpp"

Searcher::Searcher(const std::string& ip, uint16_t port, const std::shared_ptr<AbstractProtocol>& protocol, FoundPvCb foundPvCb)
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

    m_addr = {0}; // avoid using memset()
    m_addr.sin_family = AF_INET;
    m_addr.sin_port = ::htons(port);
    if (::inet_aton(ip.c_str(), reinterpret_cast<in_addr*>(&m_addr.sin_addr.s_addr)) == 0) {
        throw SocketException("invalid IP address - {errno}");
    }
}

void Searcher::processIncoming()
{
    char buffer[4096];
    struct sockaddr_in remoteAddr;
    socklen_t remoteAddrLen = sizeof(remoteAddr);
    auto recvd = ::recvfrom(m_sock, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr *>(&remoteAddr), &remoteAddrLen);
    if (recvd > 0) {
        char iocIp[20] = {0};
        ::inet_ntop(AF_INET, &remoteAddr.sin_addr, iocIp, sizeof(iocIp)-1);
        uint16_t iocPort = ::ntohs(remoteAddr.sin_port);

        LOG_DEBUG("Received UDP packet (%u bytes) from %s:%u, potential PV(s) search response", recvd, iocIp, iocPort);

        auto responses = m_protocol->parseSearchResponse({buffer, buffer+recvd});
        for (const auto& [chanId, rsp]: responses) {
            for (auto it = m_searchedPvs.begin(); it != m_searchedPvs.end(); it++) {
                if (it->second == chanId) {
                    auto pvname = it->first;

                    LOG_VERBOSE("Found %s on %s:%u", pvname.c_str(), iocIp, iocPort);
                    m_foundPvCb(pvname, iocIp, iocPort, rsp);

                    m_searchedPvs.erase(it);
                    break;
                }
            }
        }
    }
}

bool Searcher::isChanIdUsed(uint32_t chanId)
{
    for (const auto& [_, id]: m_searchedPvs) {
        if (chanId == id) {
            return true;
        }
    }
    return false;
}

void Searcher::searchPVs(const std::vector<std::string>& pvnames)
{
    std::vector<std::pair<uint32_t, std::string>> pvs;
    for (auto& pvname: pvnames) {
        uint32_t chanId;

        try {
            // We've searched for PV before, let's reuse its chanId
            chanId = m_searchedPvs.at(pvname);
        } catch (...) {

            while (true) {
                // We pick a random number and hope we can only iterace over m_searchedPvs once
                chanId = ((double)rand()/RAND_MAX) * INT32_MAX;
                if (isChanIdUsed(chanId) == false) {
                    m_searchedPvs[pvname] = chanId;
                    break;
                }
            }
        }

        pvs.emplace_back(chanId, pvname);

        // Send up to 10 PVs in one go
        if (pvs.size() == 10) {
            auto msg = m_protocol->createSearchRequest(pvs);
            ::sendto(m_sock, msg.data(), msg.size(), 0, reinterpret_cast<sockaddr *>(&m_addr), sizeof(sockaddr_in));
            pvs.clear();
        }
    }

    if (pvs.size() > 0) {
        auto msg = m_protocol->createSearchRequest(pvs);
        ::sendto(m_sock, msg.data(), msg.size(), 0, reinterpret_cast<sockaddr *>(&m_addr), sizeof(sockaddr_in));
    }
}
