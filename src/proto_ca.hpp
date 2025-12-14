/**
 * @file proto_ca.hpp
 * @brief Channel Access (CA) protocol implementation.
 */

#pragma once

#include "proto.hpp"

/**
 * @class ChannelAccess
 * @brief Implementation of the Protocol interface via Channel Access (CA) UDP protocol.
 * 
 * This class handles the construction and parsing of CA headers and payloads
 * for service discovery (Search) and connection verification (Echo).
 * It adheres to the EPICS Channel Access protocol specification v3.13/v3.14.
 */
class ChannelAccess : public Protocol {
    public:
        /**
         * @brief Creates a CA ECHO request.
         * 
         * @param includeVersion If true, includes a CA_PROTO_VERSION header before the ECHO.
         * @return Bytes The raw packet buffer.
         */
        Bytes createEchoRequest(bool includeVersion=false);

        /**
         * @brief Creates a CA SEARCH payload.
         * 
         * Constructs a packet containing as many Search requests as fit within a safe MTU.
         * 
         * @param pvs List of (CID, Name) to search for.
         * @return std::pair<Bytes, uint16_t> The packet buffer and the count of PVs included.
         */
        std::pair< Bytes, uint16_t> createSearchRequest(const std::vector<std::pair<uint32_t, std::string>> &pvs);

        /**
         * @brief Updates a search reply buffer with a specific Channel ID.
         * @note Implements Protocol::updateSearchReply.
         */
        bool updateSearchReply(Bytes& reply, uint32_t chanId);

        /**
         * @brief Updates a search reply with the finding IOC's address.
         * @note Implements Protocol::updateSearchReply.
         */
        bool updateSearchReply(Bytes& reply, const std::string& iocIp, uint16_t iocPort);

        /**
         * @brief Parses incoming CA SEARCH packets from a client.
         * @return List of (CID, Name) requested by the client.
         */
        std::vector<std::pair<uint32_t, std::string>> parseSearchRequest(const Bytes &buffer);

        /**
         * @brief Parses incoming CA SEARCH responses from an IOC.
         * @return List of (CID, ResponsePacket) found.
         */
        std::vector<std::pair<uint32_t, Bytes>> parseSearchResponse(const Bytes& buffer);

        /**
         * @brief Extracts the IOC address from a successful Search response.
         * @return Pair of (IP String, Port).
         */
        std::pair<std::string, uint16_t> parseIocAddr(const std::string& ip, uint16_t udpPort, const Bytes& buffer);
};
