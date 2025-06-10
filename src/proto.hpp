#pragma once

#include <cstdint>
#include <string>
#include <vector>

class AbstractProtocol {
    public:
        virtual std::vector<unsigned char> createEchoRequest(bool includeVersion=false) = 0;
        virtual std::pair<std::vector<unsigned char>, uint16_t> createSearchRequest(const std::vector<std::pair<uint32_t, std::string>>& pvs) = 0;
        virtual bool updateSearchReply(std::vector<unsigned char> &packet, uint32_t chanId) = 0;
        virtual bool updateSearchReply(std::vector<unsigned char> &packet, const std::string& iocIp, uint16_t iocPort) = 0;
        virtual std::vector<std::pair<uint32_t, std::string>> parseSearchRequest(const std::vector<unsigned char>& buffer) = 0;
        virtual std::vector<std::pair<uint32_t, std::vector<unsigned char>>> parseSearchResponse(const std::vector<unsigned char>& buffer) = 0;
        virtual std::pair<std::string, uint16_t> parseIocAddr(const std::string& ip, uint16_t udpPort, const std::vector<unsigned char>& buffer) = 0;
};
