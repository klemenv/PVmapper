/**
 * @file iocguard.hpp
 * @brief Active IOC health monitoring.
 */

#pragma once

#include "connection.hpp"
#include "proto.hpp"

#include <chrono>
#include <functional>
#include <memory>

/**
 * @class IocGuard
 * @brief Monitors the status of a specific IOC via TCP heartbeats.
 * 
 * Establishes and maintains a TCP connection to the IOC. If the connection drops or 
 * fails, it triggers a callback to cleanup any associated PVs in the Dispatcher.
 * This ensures that the system doesn't advertise PVs for an IOC that has gone offline.
 */
class IocGuard : public Connection {
    public:
        /**
         * @brief Callback function invoked when the IOC disconnects or times out.
         * @param iocIP IP address of the disconnected IOC.
         * @param iocPort Port of the disconnected IOC.
         */
        typedef std::function<void(const std::string& iocIP, uint16_t iocPort)> DisconnectCb;
    private:
        std::shared_ptr<Protocol> m_protocol;
        DisconnectCb m_disconnectCb;
        std::string m_ip;
        uint16_t m_port;
        std::chrono::steady_clock::time_point m_started;
        std::chrono::steady_clock::time_point m_lastRequest;
        std::chrono::steady_clock::time_point m_lastResponse;
        unsigned m_heartbeatInterval = 10;
        bool m_connected = false;
        bool m_initialized = false;

        /**
         * @brief Check if non-blocking socket is connected
         */
        bool checkConnection();

        /**
         * @brief Sends an echo/heartbeat (e.g. ECHO) request to the IOC.
         */
        void sendHeartBeat();
    public:
        /**
         * @brief Constructs an IocGuard.
         * 
         * @param iocIp IP address of the IOC.
         * @param iocPort Port of the IOC (usually TCP).
         * @param protocol Shared pointer to the protocol handler (used to create echo packets).
         * @param disconnectCb Callback invoked on connection failure.
         */
        IocGuard(const std::string& iocIp, uint16_t iocPort, const std::shared_ptr<Protocol>& protocol, DisconnectCb& disconnectCb);
        
        ~IocGuard();

        /**
         * @brief Processes incoming TCP data (Echo responses).
         * 
         * Updates the last response timestamp to keep the connection alive.
         */
        void processIncoming();

        /**
         * @brief Processes outgoing TCP data.
         * 
         * Periodically sends heartbeat packets to the IOC.
         */
        void processOutgoing();

        /**
         * @brief Gets the address of the monitored IOC.
         * @return std::pair<std::string, uint16_t> IOC IP and Port.
         */
        std::pair<std::string, uint16_t> getIocAddr() { return std::make_pair(m_ip, m_port); }
};