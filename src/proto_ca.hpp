#pragma once

#include "proto.hpp"

class ChannelAccess : public Protocol {
    public:
        Bytes createEchoRequest(bool includeVersion=false);
        std::pair< Bytes, uint16_t> createSearchRequest(const std::vector<std::pair<uint32_t, std::string>> &pvs);
        bool updateSearchReply(Bytes& reply, uint32_t chanId);
        bool updateSearchReply(Bytes& reply, const std::string& iocIp, uint16_t iocPort);
        std::vector<std::pair<uint32_t, std::string>> parseSearchRequest(const Bytes &buffer);
        std::vector<std::pair<uint32_t, Bytes>> parseSearchResponse(const Bytes& buffer);
        std::pair<std::string, uint16_t> parseIocAddr(const std::string& ip, uint16_t udpPort, const Bytes& buffer);
};
