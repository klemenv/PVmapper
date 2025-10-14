#pragma once

#include "connection.hpp"
#include "proto.hpp"

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <list>
#include <vector>

class Searcher : public Connection {
    public:
        typedef std::function<void(const std::string& pvname, const std::string& iocIp, uint16_t iocPort, const Protocol::Bytes& response)> PvFoundCb;

    protected:
        struct SearchedPV {
            uint32_t chanId;
            std::string pvname;
            std::chrono::steady_clock::time_point lastSearched; // Last time any client searched for this PV, used for book-keeping
            std::vector<uint32_t> intervals;
        };

        std::vector<uint32_t> m_searchIntervals;
        uint32_t m_chanId = 0;
        std::shared_ptr<Protocol> m_protocol;
        std::vector<std::list<SearchedPV>> m_searchedPvs;
        size_t m_currentBin = 0;
        std::chrono::steady_clock::time_point m_lastSearch;
        PvFoundCb m_foundPvCb;
        std::string m_searchIp;
        uint16_t m_searchPort;

        uint32_t getNextChanId();

    public:
        Searcher(const std::string& ip, uint16_t port, const std::vector<uint32_t>& searchIntervals, const std::shared_ptr<Protocol>& protocol, PvFoundCb& foundPvCb);
        bool addPV(const std::string& pvname);
        void removePV(const std::string& pvname);
        void processIncoming();
        void processOutgoing();
        std::pair<uint32_t, uint32_t> purgePVs(unsigned maxtime);
};
