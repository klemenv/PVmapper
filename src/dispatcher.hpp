/**
 * @file dispatcher.hpp
 * @brief Central coordinator for PV lookup and request routing.
 */

#pragma once

#include "proto_ca.hpp"
#include "iocguard.hpp"
#include "listener.hpp"
#include "searcher.hpp"

#include <memory>

/**
 * @class Dispatcher
 * @brief Main application controller.
 * 
 * The Dispatcher wires together listeners (clients), searchers (IOCs), and protocol handlers.
 * It maintains a cache of known PVs (`m_connectedPVs`) and their corresponding IOC status (`m_iocs`).
 * It handles the flow of logic:
 * 1. Client asks for PV (via Listener callback `caPvSearched`).
 * 2. Dispatcher checks cache.
 * 3. If missing, it adds PV to Searchers (`addPV`).
 * 4. When Searcher finding a PV (`caPvFound`), Dispatcher updates cache.
 */
class Dispatcher {
    private:
        typedef std::pair<std::string, uint16_t> Address;

        /**
         * @struct PvInfo
         * @brief Internal record for a cached/known PV.
         */
        struct PvInfo {
            /** 
             * Pointer to the IOC structure (may be empty) to determine IOC status.
             * This is shared by all PVs residing on the same IOC and allows for quick
             * determination if the PV is valid or not. When IOC disconnects, its
             * status will change.
             */
            std::shared_ptr<IocGuard> ioc;

            /** Raw packet response from the IOC, returned to the client */
            Protocol::Bytes response;
        };

        /**
         * @brief Supported protocol types.
         */
        enum class Proto {
            CHANNEL_ACCESS,
        };

        const Config& m_config;
        std::chrono::steady_clock::time_point m_lastPurge;
        std::shared_ptr<ChannelAccess> m_caProto;
        std::map<Address, std::shared_ptr<IocGuard>> m_iocs;
        std::vector<std::shared_ptr<Searcher>> m_caSearchers;
        std::vector<std::shared_ptr<Listener>> m_caListeners;
        std::map<std::string, PvInfo> m_connectedPVs;

        /**
         * @brief Adds a new listener for incoming client connections.
         * 
         * @param ip IP address to listen on.
         * @param port Port to listen on.
         * @param proto The protocol type this listener handles.
         */
        void addListener(const std::string& ip, uint16_t port, Proto proto);
        /**
         * @brief Adds a new searcher to discover IOCs.
         * 
         * @param ip IP address for sending search requests.
         * @param port Port for sending search requests.
         * @param proto The protocol type this searcher handles.
         * @param searchIntervals Vector of intervals (in seconds) for search retries.
         */
        void addSearcher(const std::string& ip, uint16_t port, Proto proto, const std::vector<uint32_t>& searchIntervals);

        /**
         * @brief Callback for when an IOC disconnects.
         * 
         * Marks all PVs associated with this IOC as invalid.
         * 
         * @param iocIP IP address of the disconnected IOC.
         * @param port Port of the disconnected IOC.
         */
        void iocDisconnected(const std::string& iocIP, uint16_t port);
        /**
         * @brief Callback for when a Channel Access PV is found by a searcher.
         * 
         * Caches the PV information and its associated IOC.
         * 
         * @param pvname The name of the found PV.
         * @param iocIP IP address of the IOC hosting the PV.
         * @param iocPort Port of the IOC hosting the PV.
         * @param response Raw packet response from the IOC.
         */
        void caPvFound(const std::string& pvname, const std::string& iocIP, uint16_t iocPort, const Protocol::Bytes& response);
        /**
         * @brief Callback for when a client searches for a Channel Access PV.
         * 
         * Attempts to find the PV in the cache or initiates a search.
         * 
         * @param pvname The name of the PV being searched for.
         * @param clientIP IP address of the client.
         * @param clientPort Port of the client.
         * @return Raw packet response to be sent back to the client.
         */
        Protocol::Bytes caPvSearched(const std::string &pvname, const std::string &clientIP, uint16_t clientPort);

    public:
        /**
         * @brief Constructs the Dispatcher.
         * 
         * Initializes network components based on provided configuration.
         * 
         * @param config Reference to the loaded configuration.
         */
        Dispatcher(const Config& config);

        /**
         * @brief Main processing loop.
         * 
         * Drives the ConnectionsManager loop and performs periodic maintenance tasks
         * (like purging stale PVs).
         * 
         * @param timeout Max wait time for the IO loop step.
         */
        void run(double timeout = 0.1);
};