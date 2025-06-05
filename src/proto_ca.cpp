#include "proto_ca.hpp"

#include <arpa/inet.h>

static uint16_t const CMD_VERSION =  0x0;
static uint16_t const CMD_SEARCH  =  0x6;
static uint16_t const CMD_ECHO    = 0x17;

struct Header {
    uint16_t command;
    uint16_t payloadLen;
    uint16_t dataType;
    uint16_t dataCount;
    uint32_t param1;
    uint32_t param2;
};

std::vector<unsigned char> ChannelAccess::createEchoRequest(bool includeVersion)
{
    size_t n = (includeVersion ? 2 : 1);
    std::vector<unsigned char> buffer(n * sizeof(Header), 0);
    auto hdr = reinterpret_cast<Header *>(buffer.data());
    if (includeVersion == true) {
        hdr->command = ::htons(CMD_VERSION);
        hdr->payloadLen = ::htons(0x0);
        hdr->dataType = ::htons(0x1);
        hdr->dataCount = ::htons(13);
        hdr->param1 = ::htons(0x0);
        hdr->param2 = ::htons(0x0);
        hdr++;
    }
    hdr->command = ::htons(CMD_ECHO);
    return buffer;
}

std::vector<unsigned char> ChannelAccess::createSearchRequest(const std::vector<std::pair<uint32_t, std::string>>& pvs) {
    std::vector<unsigned char> buffer(sizeof(Header));

    auto hdr = reinterpret_cast<Header *>(buffer.data());
    hdr->command = ::htons(CMD_VERSION);
    hdr->payloadLen = ::htons(0x0);
    hdr->dataType = ::htons(0x1);
    hdr->dataCount = ::htons(13);
    hdr->param1 = ::htons(0x0);
    hdr->param2 = ::htons(0x0);

    size_t offset = buffer.size();
    for (const auto& [chanId, pvname]: pvs) {
        size_t payloadLen = ((pvname.length() & 0xFFFF) + 7) & ~7; // must be aligned to 8
        buffer.resize(buffer.size() + sizeof(Header) + payloadLen);

        hdr = reinterpret_cast<Header *>(buffer.data() + offset);
        hdr->command = ::htons(CMD_SEARCH);
        hdr->payloadLen = ::htons(payloadLen);
        hdr->dataType = ::htons(0x5);
        hdr->dataCount = ::htons(13);
        hdr->param1 = ::htonl(chanId);
        hdr->param2 = ::htonl(chanId);

        auto payload = reinterpret_cast<char *>(buffer.data() + offset + sizeof(Header));
        pvname.copy(payload, pvname.length() & 0xFFFF);

        offset += sizeof(Header) + payloadLen;
    }

    return buffer;
}

bool ChannelAccess::updateSearchReply(std::vector<unsigned char> &buffer, uint32_t chanId)
{
    size_t offset = 0;
    bool updated = false;

    while ((offset + sizeof(Header)) <= buffer.size()) {
        auto hdr = reinterpret_cast<Header*>(buffer.data() + offset);
        auto command = ::ntohs(hdr->command);
        if (command == CMD_SEARCH) {
            hdr->param2 = ::htonl(chanId);
            updated = true;
        }
        offset += sizeof(Header) + ::ntohs(hdr->payloadLen);
    }

    return updated;
}

bool ChannelAccess::updateSearchReply(std::vector<unsigned char> &buffer, const std::string& iocIp, uint16_t iocPort)
{
    size_t offset = 0;
    bool updated = false;

    while ((offset + sizeof(Header)) <= buffer.size()) {
        auto hdr = reinterpret_cast<Header*>(buffer.data() + offset);
        auto command = ::ntohs(hdr->command);
        if (command == CMD_SEARCH) {
            hdr->dataType = ::htons(iocPort);
            ::inet_aton(iocIp.c_str(), reinterpret_cast<in_addr *>(&hdr->param1));
            updated = true;
        }
        offset += sizeof(Header) + ::ntohs(hdr->payloadLen);
    }

    return updated;
}

std::vector<std::pair<uint32_t, std::string>> ChannelAccess::parseSearchRequest(const std::vector<unsigned char>& buffer) {
    std::vector<std::pair<uint32_t, std::string>> pvs;
    size_t offset = 0;

    // Helper function to copy only non-null characters out from the buffer
    auto copyStr = [](const char* s, uint32_t len) {
        while (len > 0) {
            if (s[len-1] != '\0') {
                break;
            }
            len--;
        }
        return std::string(s, len);
    };

    while ((offset + sizeof(Header)) <= buffer.size()) {
        auto hdr = reinterpret_cast<const Header*>(buffer.data() + offset);
        auto payloadLen = ::ntohs(hdr->payloadLen);

        if (hdr->command == ::htons(CMD_SEARCH) && (offset + sizeof(Header) + payloadLen) <= buffer.size()) {
            auto payload = reinterpret_cast<const char *>(buffer.data() + offset + sizeof(Header));
            uint32_t chanId = ::ntohl(hdr->param1);
            auto pv = copyStr(payload, payloadLen);
            pvs.emplace_back(chanId, pv);
        }

        offset += sizeof(Header) + payloadLen;
    }

    return pvs;
}

std::vector<std::pair<uint32_t, std::vector<unsigned char>>> ChannelAccess::parseSearchResponse(const std::vector<unsigned char>& buffer)
{
    std::vector<std::pair<uint32_t, std::vector<unsigned char>>> searches;

    const Header* version = nullptr;
    size_t offset = 0;
    while ((offset + sizeof(Header)) <= buffer.size()) {
        auto hdr = reinterpret_cast<const Header*>(buffer.data() + offset);
        uint16_t command = ::ntohs(hdr->command);
        auto payloadLen = ::ntohs(hdr->payloadLen);

        if (command == CMD_VERSION) {
            version = hdr;
        } else if (command == CMD_SEARCH && (offset + sizeof(Header) + payloadLen) <= buffer.size()) {
            if (payloadLen == 8 && hdr->dataCount == 0) {
                uint32_t chanId = ::ntohl(hdr->param2);
                std::vector<unsigned char> rsp;
                if (version) {
                    auto ver = reinterpret_cast<const char *>(version);
                    rsp.assign(ver, ver + sizeof(Header));
                }
                for (size_t i = offset; i < offset + sizeof(Header) + 8; i++) {
                    rsp.push_back(buffer[i]);
                }
                searches.emplace_back(chanId, rsp);
            }
        }

        offset += sizeof(Header) + payloadLen;
    }

    return searches;
}

std::pair<std::string, uint16_t> ChannelAccess::parseIocAddr(const std::string& ip, [[maybe_unused]] uint16_t udpPort, const std::vector<unsigned char>& buffer)
{
    size_t offset = 0;
    while ((offset + sizeof(Header)) <= buffer.size()) {
        auto hdr = reinterpret_cast<const Header*>(buffer.data() + offset);
        uint16_t command = ::ntohs(hdr->command);
        auto payloadLen = ::ntohs(hdr->payloadLen);

        if (command == CMD_SEARCH && (offset + sizeof(Header) + payloadLen) <= buffer.size()) {
            if (payloadLen == 8 && hdr->dataCount == 0) {
                return std::make_pair(ip, ::ntohs(hdr->dataType));
            }
        }

        offset += sizeof(Header) + payloadLen;
    }

    return std::make_pair("", 0);
}