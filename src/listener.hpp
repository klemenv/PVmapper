/**
 * @file listener.hpp
 * @brief Handles incoming search requests from clients.
 */

#pragma once

#include "config.hpp"
#include "proto.hpp"
#include "connection.hpp"

#include <functional>
#include <memory>
#include <regex>
#include <string>
#include <vector>

/**
 * @class Listener
 * @brief UDP Listener for client search broadcasts.
 * 
 * Binds to a specific network interface/port to receive Channel Access search requests.
 * It validates requests against the AccessControl rules and, if allowed, attempts
 * to resolve the PV via the provided callback. If resolved, it sends a reply back to the client.
 */
class Listener : public Connection {
    public:
        /**
         * @brief Callback invoked when a PV search request is received and allowed.
         * 
         * @param pvname The name of the PV being searched.
         * @param clientIP The IP address of the requesting client.
         * @param clientPort The UDP port of the requesting client.
         * @return Protocol::Bytes The response packet to send back (if any).
         */
        typedef std::function<Protocol::Bytes (const std::string & /*pvname*/, const std::string & /*client IP*/, uint16_t /*client port*/)> PvSearchedCb;

    private:
        const AccessControl& m_accessControl;
        std::shared_ptr<Protocol> m_protocol;
        PvSearchedCb m_searchPvCb;

        /**
         * @brief Checks if the client is authorized to search for the given PV.
         * 
         * @param pvname The PV name.
         * @param client The client IP.
         * @param port The client port.
         * @return bool True if access is granted, False otherwise.
         */
        bool checkAccessControl(const std::string &pvname, const std::string &client, uint16_t port);

    public:
   
        /**
         * @brief Constructs a Listener.
         * 
         * @param ip The local IP address to bind to.
         * @param port The local UDP port to bind to.
         * @param accessControl Reference to the security policies.
         * @param protocol Shared pointer to the protocol implementation (CA).
         * @param cb Callback function to query PV resolution.
         */
        Listener(const std::string& ip, uint16_t port, const AccessControl& accessControl, const std::shared_ptr<Protocol>& protocol, PvSearchedCb& cb);

        /**
         * @brief Process incoming UDP packets.
         * 
         * Reads from the socket, parses the search request, checks ACLs, 
         * invokes the callback, and sends the response if found.
         */
        void processIncoming();
};