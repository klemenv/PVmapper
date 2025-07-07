#pragma once

#include <cstdint>
#include <string>
#include <vector>

class Protocol {
    public:
        typedef std::basic_string<unsigned char> Bytes;

        virtual Bytes createEchoRequest(bool includeVersion = false) = 0;
        virtual std::pair<Bytes, uint16_t> createSearchRequest(const std::vector<std::pair<uint32_t, std::string>>& pvs) = 0;
        virtual bool updateSearchReply(Bytes& reply, uint32_t chanId) = 0;
        virtual bool updateSearchReply(Bytes& reply, const std::string& iocIp, uint16_t iocPort) = 0;
        virtual std::vector<Bytes> combineSearchReplies(const std::vector<Bytes>& replies) = 0;
        virtual std::vector<std::pair<uint32_t, std::string>> parseSearchRequest(const Bytes& buffer) = 0;
        virtual std::vector<std::pair<uint32_t, Bytes>> parseSearchResponse(const Bytes& buffer) = 0;
        virtual std::pair<std::string, uint16_t> parseIocAddr(const std::string& ip, uint16_t udpPort, const Bytes& buffer) = 0;
};
