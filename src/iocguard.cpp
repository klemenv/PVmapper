#include "logging.hpp"
#include "iocguard.hpp"

#include <fcntl.h>
#include <unistd.h>

IocGuard::IocGuard(const std::string& iocIp, uint16_t iocPort, const std::shared_ptr<AbstractProtocol>& protocol, DisconnectCb& disconnectCb)
    : m_protocol(protocol)
    , m_disconnectCb(disconnectCb)
    , m_ip(iocIp)
    , m_port(iocPort)
{
    m_sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_sock < 0) {
        throw SocketException("failed to create socket", errno);
    }

    if (::fcntl(m_sock, F_SETFL, fcntl(m_sock, F_GETFL, 0) | O_NONBLOCK) == -1) {
        throw SocketException("failed to set socket non-blocking", errno);
    }

    m_addr = {0}; // avoid using memset()
    m_addr.sin_family = AF_INET;
    m_addr.sin_port = ::htons(iocPort);
    if (::inet_aton(iocIp.c_str(), reinterpret_cast<in_addr*>(&m_addr.sin_addr.s_addr)) == 0) {
        int err = errno;
        ::close(m_sock);
        m_sock = -1;
        throw SocketException("invalid IP address", err);
    }

    if (::connect(m_sock, reinterpret_cast<sockaddr*>(&m_addr), sizeof(m_addr)) != 0 && errno != EINPROGRESS) {
        int err = errno;
        ::close(m_sock);
        m_sock = -1;
        throw SocketException("failed to connect", err);
    }

    auto msg = m_protocol->createEchoRequest(true);
    if (::send(m_sock, msg.data(), msg.size(), 0) == -1) {
        int err = errno;
        ::close(m_sock);
        m_sock = -1;
        throw SocketException("failed to establish connection", err);
    }

    m_lastRequest = std::chrono::steady_clock::now();
    m_lastResponse = std::chrono::steady_clock::now();
}

IocGuard::~IocGuard()
{
    if (m_sock != -1) {
        ::close(m_sock);
    }
}

void IocGuard::processIncoming()
{
    char buffer[4096];
    auto recvd = ::recv(m_sock, buffer, sizeof(buffer), 0);
    if (recvd > 0) {
        LOG_DEBUG("Received heart-beat response from IOC ", m_ip, ":", m_port);
        m_lastResponse = std::chrono::steady_clock::now();
    } else {
        ::close(m_sock);
        m_sock = -1;
        m_disconnectCb(m_ip, m_port);
        LOG_INFO("IOC ", m_ip, ":", m_port, " appears to have closed socket, disconnecting...");
    }
}

void IocGuard::processOutgoing()
{
    auto diff = (std::chrono::steady_clock::now() - m_lastRequest);
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(diff).count();
    if (duration > m_heartbeatInterval) {
        sendHeartBeat();
    }
}

void IocGuard::sendHeartBeat()
{
    if (m_sock != -1) {
        if (m_lastRequest < m_lastResponse) {
            auto msg = m_protocol->createEchoRequest(false);
            if (::send(m_sock, msg.data(), msg.size(), 0) > 0) {
                LOG_DEBUG("Sent heart-beat request to ", m_ip, ":", m_port);
                m_lastRequest = std::chrono::steady_clock::now();
                return;
            } else {
                LOG_INFO("Failed to send heart-beat to IOC ", m_ip, ":", m_port, ", disconnecting...");
            }
        } else {
            LOG_INFO("Didn't received last heart-beat response from IOC ", m_ip, ":", m_port, ", disconnecting...");
        }

        ::close(m_sock);
        m_sock = -1;
        m_disconnectCb(m_ip, m_port);
    }
}