/**
 * @file connmgr.hpp
 * @brief Singleton-like manager for network connections.
 */

#pragma once

#include "connection.hpp"

#include <memory>
#include <vector>

/**
 * @class ConnectionsManager
 * @brief Manages the lifecycle and IO processing of multiple Connection objects.
 * 
 * This class acts as a reactor/dispatcher. It maintains a list of active connections
 * and uses `select` (or equivalent) to poll them for incoming data, invoking
 * their processing methods when ready.
 */
class ConnectionsManager {
    private:
        std::vector<std::shared_ptr<Connection>> m_connections;

    public:
        /**
         * @brief Registers a connection with the manager.
         * @param connection Shared pointer to the connection instance.
         */
        static void add(const std::shared_ptr<Connection>& connection);

        /**
         * @brief Unregisters a connection.
         * @param connection Shared pointer to the connection instance to remove.
         */
        static void remove(const std::shared_ptr<Connection>& connection);

        /**
         * @brief Runs the main IO loop for a single iteration/slice.
         * 
         * Polls all registered connections for read/write readiness.
         * 
         * @param timeout Maximum time to wait for IO events in seconds (default 0.1s).
         */
        static void run(double timeout = 0.1);
};