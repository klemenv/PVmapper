#pragma once

#include "connection.hpp"
#include "proto.hpp"

#include <chrono>
#include <memory>

class IocGuard : public Connection {
    public:
        typedef void(*DisconnectCb)(const std::string&, uint16_t);
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
        IocGuard(const std::string& iocIp, uint16_t iocPort, const std::shared_ptr<AbstractProtocol>& protocol, DisconnectCb disconnectCb);
        ~IocGuard();
        void processIncoming();
        void processOutgoing();
};