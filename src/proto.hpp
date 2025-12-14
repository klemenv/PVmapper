/**
 * @file proto.hpp
 * @brief Abstract interface for protocol implementations.
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

/**
 * @class Protocol
 * @brief Abstract base class defining the interface for network protocol handling.
 * 
 * This class provides a contract for constructing and parsing protocol packets,
 * specifically tailored for Channel Access (CA), PV Access (PVA) style communication
  (Search, Echo, etc.).
 * Implementations (like ChannelAccess and PvAccess) are responsible for the specific byte-level encoding.
 */
class Protocol {
    public:
        /**
         * @brief Type alias for a raw byte buffer (using unsigned char).
         */
        typedef std::basic_string<unsigned char> Bytes;

        /**
         * @brief Creates an ECHO request packet.
         * 
         * @param includeVersion If true, creates a packet that includes a Version header 
         *                       before the Echo header.
         * @return Bytes The constructed packet buffer.
         */
        virtual Bytes createEchoRequest(bool includeVersion = false) = 0;

        /**
         * @brief Creates a SEARCH request packet for a list of PVs.
         * 
         * @param pvs A vector of pairs (Channel CID, PV Name) to search for.
         * @return std::pair<Bytes, uint16_t> A pair containing:
         *         - The constructed packet buffer.
         *         - The count of PVs that fit into the packet.
         */
        virtual std::pair<Bytes, uint16_t> createSearchRequest(const std::vector<std::pair<uint32_t, std::string>>& pvs) = 0;

        /**
         * @brief Updates a SEARCH reply packet with the given channel ID.
         * 
         * @param reply The reply packet to update.
         * @param chanId The channel ID to add to the reply.
         * @return bool True if the update was successful, false otherwise.
         */
        virtual bool updateSearchReply(Bytes& reply, uint32_t chanId) = 0;

        /**
         * @brief Updates a SEARCH reply packet with the given IOC IP and port.
         * 
         * @param reply The reply packet to update.
         * @param iocIp The IOC IP address to add to the reply.
         * @param iocPort The IOC port to add to the reply.
         * @return bool True if the update was successful, false otherwise.
         */
        virtual bool updateSearchReply(Bytes& reply, const std::string& iocIp, uint16_t iocPort) = 0;

        /**
         * @brief Parses a SEARCH request packet into a list of PVs.
         * 
         * @param buffer The buffer containing the SEARCH request packet.
         * @return std::vector<std::pair<uint32_t, std::string>> A vector of pairs (Channel CID, PV Name).
         */
        virtual std::vector<std::pair<uint32_t, std::string>> parseSearchRequest(const Bytes& buffer) = 0;

        /**
         * @brief Parses a SEARCH reply packet into a list of PVs and their IOC addresses.
         * 
         * @param buffer The buffer containing the SEARCH reply packet.
         * @return std::vector<std::pair<uint32_t, Bytes>> A vector of pairs (Channel CID, IOC Address).
         */
        virtual std::vector<std::pair<uint32_t, Bytes>> parseSearchResponse(const Bytes& buffer) = 0;

        /**
         * @brief Parses an IOC address from a buffer.
         * 
         * @param ip The IP address of the IOC.
         * @param udpPort The UDP port of the IOC.
         * @param buffer The buffer containing the IOC address.
         * @return std::pair<std::string, uint16_t> A pair containing the IOC IP and port.
         */
        virtual std::pair<std::string, uint16_t> parseIocAddr(const std::string& ip, uint16_t udpPort, const Bytes& buffer) = 0;
};
