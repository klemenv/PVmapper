#include "listener.hpp"
#include "logging.hpp"

#include <sys/socket.h>

Listener::Listener(const std::string& ip, uint16_t port, const AccessControl& accessControl, const std::shared_ptr<AbstractProtocol>& protocol, SearchPvCb cb)
    : m_accessControl(accessControl)
    , m_protocol(protocol)
    , m_searchPvCb(cb)
{
    m_sock = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (m_sock < 0) {
        throw SocketException("failed to create socket - {errno}");
    }

/*
    int optval = 1;
    if (::setsockopt(m_sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) != 0) {
        throw SocketException("can't set reuse port option - {errno}");
    }
*/

    m_addr = {0}; // avoid using memset()
    m_addr.sin_family = AF_INET;
    m_addr.sin_port = ::htons(port);
    if (ip.empty()) {
        m_addr.sin_addr.s_addr = INADDR_ANY;
    } else if (::inet_aton(ip.c_str(), reinterpret_cast<in_addr*>(&m_addr.sin_addr.s_addr)) == 0) {
        throw SocketException("invalid IP address - {errno}");
    }

    if (::bind(m_sock, reinterpret_cast<sockaddr *>(&m_addr), sizeof(m_addr)) < 0 )  {
        throw SocketException("failed to bind to address - {errno}");
    }
}

void Listener::processIncoming() {
    unsigned char buffer[4096];
    struct sockaddr_in remoteAddr;
    socklen_t remoteAddrLen = sizeof(remoteAddr);

    auto recvd = ::recvfrom(m_sock, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr *>(&remoteAddr), &remoteAddrLen);
    if (recvd > 0) {
        char clientIp[20] = {0};
        ::inet_ntop(AF_INET, &remoteAddr.sin_addr, clientIp, sizeof(clientIp)-1);
        uint16_t clientPort = ::ntohs(remoteAddr.sin_port);

        LOG_DEBUG("Received UDP packet (%u bytes) from %s:%u, potential PV(s) search request", recvd, clientIp, clientPort);

        auto pvs = m_protocol->parseSearchRequest({buffer, buffer + recvd});
        for (const auto& [chanId, pvname]: pvs) {

            LOG_VERBOSE("%s:%u searching for %s", clientIp, clientPort, pvname.c_str());

            if (checkAccessControl(pvname, clientIp)) {

                auto rsp = m_searchPvCb(pvname, clientIp, clientPort);
                if (rsp.empty() == false) {
                    m_protocol->updateSearchRequest(rsp, chanId);
                    ::sendto(m_sock, rsp.data(), rsp.size(), 0, reinterpret_cast<sockaddr *>(&remoteAddr), remoteAddrLen);
                }
            }
        }
    }
}

bool Listener::checkAccessControl(const std::string& pvname_, const std::string& client)
{
    auto pvname = pvname_;

    // Remove the optional .FIELD from PV names
    auto pos = pvname.find_last_of('.');
    if (pos != std::string::npos) {
        pvname.erase(pos);
    }

    // Run the PV rules first
    for (auto& rule: m_accessControl.pvs) {
        // The first rule that matches will take place, either ALLOW or DENY
        if (regex_match(pvname, rule.regex)) {
            if (rule.action == AccessControl::ALLOW) {
                break;
            } else if (rule.action == AccessControl::DENY) {
                LOG_VERBOSE("Rejected request from ", client, " searching for PV ", pvname, " due to '", rule.text, "' rule");
                return false;
            }
        }

        // Try next rule until there's any. If there's no `DENY PV *' rule
        // in the end, `ALLOW PV *' is assumed.
    }

    // Run rules for client IPs - CA only supports IPv4
    for (auto& rule: m_accessControl.clients) {
        // The first rule that matches will take place, either ALLOW or DENY
        if (regex_match(client, rule.regex)) {
            if (rule.action == AccessControl::ALLOW) {
                break;
            } else if (rule.action == AccessControl::DENY) {
                LOG_VERBOSE("Rejected request from ", client, " searching for PV ", pvname, " due to '", rule.text, "' rule");
                return false;
            }
        }

        // Try next rule until there's any. If there's no `DENY CLIENT *' rule
        // in the end, `ALLOW CLIENT *' is assumed.
    }

    return true;
}