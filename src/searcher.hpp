#pragma once

#include "connection.hpp"
#include "proto.hpp"

#include <functional>
#include <map>
#include <memory>

class Searcher : public Connection {
    public:
        typedef std::function<void(const std::string& pvname, const std::string& iocIp, uint16_t iocPort, const std::vector<unsigned char>& response)> FoundPvCb;

    private:
        uint32_t m_chanId = 0;
        std::shared_ptr<AbstractProtocol> m_protocol;
        std::map<std::string, uint32_t> m_searchedPvs;
        FoundPvCb m_foundPvCb;

        bool isChanIdUsed(uint32_t chanId);

    public:
        Searcher(const std::string& ip, uint16_t port, const std::shared_ptr<AbstractProtocol>& protocol, FoundPvCb& foundPvCb);
        void processIncoming();
        void searchPVs(const std::vector<std::string>& pvs);
};