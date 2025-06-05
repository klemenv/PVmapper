#pragma once

#include "connection.hpp"
#include "proto.hpp"

#include <chrono>
#include <functional>
#include <memory>

class IocGuard : public Connection {
    public:
        typedef std::function<void(const std::string& iocIP, uint16_t iocPort)> DisconnectCb;
    private:
        std::shared_ptr<AbstractProtocol> m_protocol;
        DisconnectCb m_disconnectCb;
        std::string m_ip;
        uint16_t m_port;
        std::chrono::steady_clock::time_point m_lastRequest;
        std::chrono::steady_clock::time_point m_lastResponse;
        unsigned m_heartbeatInterval = 3;

        void sendHeartBeat();
    public:
        IocGuard(const std::string& iocIp, uint16_t iocPort, const std::shared_ptr<AbstractProtocol>& protocol, DisconnectCb& disconnectCb);
        ~IocGuard();
        void processIncoming();
        void processOutgoing();
        std::pair<std::string, uint16_t> getIocAddr() { return std::make_pair(m_ip, m_port); }
};