#pragma once

#include "proto.hpp"

class ChannelAccess : public AbstractProtocol {
    public:
        std::vector<unsigned char> createEchoRequest(bool includeVersion=false);
        std::pair< std::vector<unsigned char>, uint16_t> createSearchRequest(const std::vector<std::pair<uint32_t, std::string>> &pvs);
        bool updateSearchReply(std::vector<unsigned char> &request, uint32_t chanId);
        bool updateSearchReply(std::vector<unsigned char> &packet, const std::string& iocIp, uint16_t iocPort);
        std::vector<std::pair<uint32_t, std::string>> parseSearchRequest(const std::vector<unsigned char> &buffer);
        std::vector<std::pair<uint32_t, std::vector<unsigned char>>> parseSearchResponse(const std::vector<unsigned char>& buffer);
        std::pair<std::string, uint16_t> parseIocAddr(const std::string& ip, uint16_t udpPort, const std::vector<unsigned char>& buffer);
};
