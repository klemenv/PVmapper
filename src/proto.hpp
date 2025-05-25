#pragma once

#include <cstdint>
#include <string>
#include <vector>

class AbstractProtocol {
    public:
        virtual std::vector<unsigned char> createEchoRequest(bool includeVersion=false) = 0;
        virtual std::vector<unsigned char> createSearchRequest(const std::vector<std::pair<uint32_t, std::string>>& pvs) = 0;
        virtual bool updateSearchRequest(std::vector<unsigned char> &packet, uint32_t chanId) = 0;
        virtual std::vector<std::pair<uint32_t, std::string>> parseSearchRequest(const std::vector<unsigned char>& buffer) = 0;
        virtual std::vector<std::pair<uint32_t, std::vector<unsigned char>>> parseSearchResponse(const std::vector<unsigned char>& buffer) = 0;
};
