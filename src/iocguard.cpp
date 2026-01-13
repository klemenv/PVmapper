#include "dnscache.hpp"
#include "logging.hpp"
#include "iocguard.hpp"

#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

IocGuard::IocGuard(const std::string& iocIp, uint16_t iocPort, const std::shared_ptr<Protocol>& protocol, DisconnectCb& disconnectCb)
    : m_protocol(protocol)
    , m_disconnectCb(disconnectCb)
    , m_ip(iocIp)
    , m_port(iocPort)
{
    m_sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_sock < 0) {
        throw SocketException("create socket", errno);
    }

    m_addr = {}; // avoid using memset()
    m_addr.sin_family = AF_INET;
    m_addr.sin_port = ::htons(iocPort);
    if (::inet_aton(iocIp.c_str(), reinterpret_cast<in_addr*>(&m_addr.sin_addr.s_addr)) == 0) {
        int err = errno;
        ::close(m_sock);
        m_sock = -1;
        throw SocketException("invalid IP address", err);
    }

    if (::fcntl(m_sock, F_SETFL, fcntl(m_sock, F_GETFL, 0) | O_NONBLOCK) == -1) {
        throw SocketException("set socket non-blocking", errno);
    }

    if (::connect(m_sock, reinterpret_cast<sockaddr*>(&m_addr), sizeof(m_addr)) != 0 && errno != EINPROGRESS) {
        int err = errno;
        ::close(m_sock);
        m_sock = -1;
        throw SocketException("connecting ", err);
    }

    m_started = std::chrono::steady_clock::now();
}

IocGuard::~IocGuard()
{
    if (m_sock != -1) {
        ::close(m_sock);
    }
}

void IocGuard::processIncoming()
{
    if (m_sock != -1) {
        char buffer[4096];
        auto recvd = ::recv(m_sock, buffer, sizeof(buffer), 0);
        if (recvd > 0) {
            LOG_VERBOSE("Received heart-beat response from IOC ", DnsCache::resolveIP(m_ip), ":", m_port);
            m_lastResponse = std::chrono::steady_clock::now();
            m_initialized = true;
        } else {
            ::close(m_sock);
            m_sock = -1;
            m_disconnectCb(m_ip, m_port);
            if (recvd == 0) {
                LOG_INFO("IOC ", DnsCache::resolveIP(m_ip), ":", m_port, " appears to have closed socket, disconnecting...");
            } else {
                LOG_INFO("Error receiving data from IOC ", DnsCache::resolveIP(m_ip), ":", m_port, ", disconnecting...");
            }
        }
    }
}

void IocGuard::processOutgoing()
{
    if (m_sock != -1 && checkConnection() == true) {
        auto diff = (std::chrono::steady_clock::now() - m_lastRequest);
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(diff).count();
        if (duration > m_heartbeatInterval) {
            sendHeartBeat();
        }
    }
}

bool IocGuard::checkConnection()
{
    if (m_connected == false) {
        struct pollfd pfd;
        pfd.fd = m_sock;
        pfd.events = POLLOUT;

        auto pollret = ::poll(&pfd, 1, 0);
        if (pollret <= 0) {
            auto diff = (std::chrono::steady_clock::now() - m_started);
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(diff).count();
            if (duration > 5) {
                LOG_INFO("Failed to connect to IOC ", DnsCache::resolveIP(m_ip), ":", m_port, " in 5 seconds, giving up...");
                ::close(m_sock);
                m_sock = -1;
                m_disconnectCb(m_ip, m_port);
            }
            return false;
        }

        // poll() returned succesfully, we must be connected
        m_connected = true;
        m_lastResponse = std::chrono::steady_clock::now();
    }

    return true;
}

void IocGuard::sendHeartBeat()
{
    if (m_lastRequest < m_lastResponse) {
        auto msg = m_protocol->createEchoRequest(!m_initialized);
        if (::send(m_sock, msg.data(), msg.size(), 0) > 0) {
            LOG_DEBUG("Sent heart-beat request to ", DnsCache::resolveIP(m_ip), ":", m_port);
            m_lastRequest = std::chrono::steady_clock::now();
            return;
        } else {
            LOG_INFO("Failed to send heart-beat to IOC ", DnsCache::resolveIP(m_ip), ":", m_port, ", disconnecting...");
        }
    } else {
        LOG_INFO("Didn't receive last heart-beat response from IOC ", DnsCache::resolveIP(m_ip), ":", m_port, ", disconnecting...");
    }

    ::close(m_sock);
    m_sock = -1;
    m_disconnectCb(m_ip, m_port);
}
