/**
 * @file searcher.hpp
 * @brief Handles periodic broadcasting and discovery of PVs.
 */

#pragma once

#include "connection.hpp"
#include "proto.hpp"

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <list>
#include <vector>

/**
 * @class Searcher
 * @brief Manages the search/discovery phase of PV connection.
 *
 * This class is responsible for broadcasting search requests for PVs on the network.
 * It implements an exponential backoff strategy (via configured intervals) to avoid
 * flooding the network. It manages a socket for both sending UDP broadcasts and
 * receiving search responses.
 */
class Searcher : public Connection {
    public:
        /**
         * @brief Callback invoked when a PV is successfully found.
         * @param pvname The name of the PV found.
         * @param iocIp The IP address of the responding IOC.
         * @param iocPort The port of the responding IOC.
         * @param response The raw response packet containing additional metadata.
         */
        typedef std::function<void(const std::string& pvname, const std::string& iocIp, uint16_t iocPort, const Protocol::Bytes& response)> PvFoundCb;

    protected:
        /**
         * @struct SearchedPV
         * @brief Internal tracking structure for a PV being searched.
         */
        struct SearchedPV {
            uint32_t chanId;                    ///< Unique channel ID assigned for the search session.
            std::string pvname;                 ///< Name of the PV.
            std::chrono::steady_clock::time_point lastSearched; ///< Timestamp of the last search/allocation.
            std::vector<uint32_t> intervals;    ///< Remaining backoff intervals key.
        };

        std::vector<uint32_t> m_searchIntervals; ///< Configured backoff intervals.
        uint32_t m_chanId = 0;                   ///< Counter for generating unique Channel IDs.
        std::shared_ptr<Protocol> m_protocol;    ///< Protocol handler (CA/PVA).
        std::vector<std::list<SearchedPV>> m_searchedPvs; ///< Bins of PVs scheduled for future searches.
        size_t m_currentBin = 0;                 ///< Current bin index being processed.
        std::chrono::steady_clock::time_point m_lastSearch; ///< Timestamp of the last outgoing broadcast.
        PvFoundCb m_foundPvCb;                   ///< User callback for found PVs.
        std::string m_searchIp;                  ///< Broadcast IP address.
        uint16_t m_searchPort;                   ///< Broadcast port.

        /**
         * @brief Generates a unique Channel ID for a new search.
         * @return A unique 32-bit integer.
         */
        uint32_t getNextChanId();

    public:
        /**
         * @brief Constructs a Searcher.
         * 
         * @param ip Broadcast IP address (e.g., "192.168.1.255").
         * @param port Broadcast port.
         * @param searchIntervals List of intervals (in arbitrary units, often ~10Hz steps) for backoff.
         * @param protocol Shared pointer to the protocol implementation.
         * @param foundPvCb Callback for when a PV is found.
         */
        Searcher(const std::string& ip, uint16_t port, const std::vector<uint32_t>& searchIntervals, const std::shared_ptr<Protocol>& protocol, PvFoundCb& foundPvCb);

        /**
         * @brief Adds a PV to the search list.
         * 
         * @param pvname Name of the PV to find.
         * @return bool True if the PV was added, False if it was already being searched.
         */
        bool addPV(const std::string& pvname);

        /**
         * @brief Removes a PV from the search list.
         * @param pvname Name of the PV to stop searching for.
         */
        void removePV(const std::string& pvname);

        /**
         * @brief Processes incoming UDP packets.
         * 
         * Reads from the socket, parses responses, and triggers callbacks if
         * any searched PVs are found.
         */
        void processIncoming();

        /**
         * @brief Processes outgoing UDP broadcasts.
         * 
         * Iterates through the current bin of PVs, sends search requests,
         * and schedules them for the next interval bin.
         */
        void processOutgoing();

        /**
         * @brief Purges stale PVs from the search list.
         * 
         * Removes PVs that haven't been "touched" or re-added by a client
         * within the specified time, effectively implementing a timeout mechanism
         * for unreferenced PVs.
         * 
         * @param maxtime Maximum duration (in seconds) to keep a searched PV active.
         * @return std::pair<uint32_t, uint32_t> Pair of (Purged Count, Remaining Count).
         */
        std::pair<uint32_t, uint32_t> purgePVs(unsigned maxtime);
};
