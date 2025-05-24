#pragma once

#include "proto.hpp"

class ChannelAccess : public AbstractProtocol {
    public:
        std::vector<unsigned char> createSearchRequest(const std::vector<std::pair<uint32_t, std::string>> &pvs);
        bool updateSearchRequest(std::vector<unsigned char> &request, uint32_t chanId);
        std::vector<std::pair<uint32_t, std::string>> parseSearchRequest(const std::vector<unsigned char> &buffer);
        std::vector<std::pair<uint32_t, std::vector<unsigned char>>> parseSearchResponse(const std::vector<unsigned char>& buffer);
};
