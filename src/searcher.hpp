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
        typedef std::function<void(const std::string& pvname, const std::string& iocIp, uint16_t iocPort, const std::vector<unsigned char>& response)> PvFoundCb;

    private:
        struct SearchedPV {
            uint32_t chanId;
            std::string pvname;
            std::chrono::steady_clock::time_point nextSearch;
            uint32_t retries = 0;
        };

        uint32_t m_searchInterval = 10;
        uint32_t m_chanId = 0;
        std::shared_ptr<AbstractProtocol> m_protocol;
        std::list<SearchedPV> m_searchedPvs;
        PvFoundCb m_foundPvCb;
        std::string m_searchIp;
        uint16_t m_searchPort;

        uint32_t getNextChanId();

    public:
        Searcher(const std::string& ip, uint16_t port, const std::shared_ptr<AbstractProtocol>& protocol, PvFoundCb& foundPvCb);
        bool addPV(const std::string &pvname);
        void processIncoming();
        void processOutgoing();
};
