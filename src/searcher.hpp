#pragma once

#include "connection.hpp"
#include "proto.hpp"

#include <functional>
#include <map>
#include <memory>

class Searcher : public Connection {
    public:
        typedef void(*FoundPvCb)(const std::string&, const std::string&, uint16_t, const std::vector<unsigned char>&);

    private:
        uint32_t m_chanId = 0;
        std::shared_ptr<AbstractProtocol> m_protocol;
        std::map<std::string, uint32_t> m_searchedPvs;
        FoundPvCb m_foundPvCb;

        bool isChanIdUsed(uint32_t chanId);

    public:
        Searcher(const std::string& ip, uint16_t port, const std::shared_ptr<AbstractProtocol>& protocol, FoundPvCb foundPvCb);
        void processIncoming();
        void searchPVs(const std::vector<std::string>& pvs);
};