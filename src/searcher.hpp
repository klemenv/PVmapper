#pragma once

#include "connection.hpp"
#include "proto.hpp"

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <list>

class Searcher : public Connection {
    public:
        typedef std::function<void(const std::string& pvname, const std::string& iocIp, uint16_t iocPort, const Protocol::Bytes& response)> PvFoundCb;

    private:
        struct SearchedPV {
            uint32_t chanId;
            std::string pvname;
            std::chrono::steady_clock::time_point nextSearch;
            std::chrono::steady_clock::time_point lastSearched; // Last time any client searched for this PV, used for book-keeping
            uint32_t retries = 0;
        };

        uint32_t m_searchInterval = 10;
        uint32_t m_chanId = 0;
        std::shared_ptr<Protocol> m_protocol;
        std::list<SearchedPV> m_searchedPvs;
        PvFoundCb m_foundPvCb;
        std::string m_searchIp;
        uint16_t m_searchPort;

        uint32_t getNextChanId();

        /**
         * Schedule the next search of the PV
         * 
         * Takes the PV from the m_searchedPvs, increments the SearchedPV.nextSearch 
         * timestamp by the specified delay, and inserts it back to the sorted list.
         * The algorithm uses the move semantics for efficiency, and works best
         * when the PV is in the front of the list and after incrementing the
         * timestamp, it should be inserted at the end of the list. In this case the
         * the performance is O(1). Luckily that's how the function is used.
         */
        void scheduleNextSearch(const std::string& pvname, uint32_t delay);

    public:
        Searcher(const std::string& ip, uint16_t port, uint32_t searchInterval, const std::shared_ptr<Protocol>& protocol, PvFoundCb& foundPvCb);
        bool addPV(const std::string& pvname);
        void removePV(const std::string& pvname);
        void processIncoming();
        void processOutgoing();
        void purgePVs(unsigned maxtime);
};
