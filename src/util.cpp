#include "util.hpp"

#include <stdexcept>

namespace Util {

    struct sockaddr_in chIdToAddr(chid chanId)
    {
        struct sockaddr_in addr;

        char buf[128] = "";
        if (ca_get_host_name(chanId, buf, sizeof(buf)) == 0) {
            throw std::runtime_error("Failed to resolve IOC address");
        }
    
        if (aToIPAddr(buf, 5064, &addr) != 0) {
            throw std::runtime_error("Failed to resolve IOC address");
        }
    
        return addr;
    }

    std::string addrToHostName(const struct sockaddr_in& addr)
    {
        char buf[128] = "";
        if (ipAddrToA(&addr, buf, sizeof(buf)) == 0) {
            throw std::runtime_error("Failed to convert IOC address to hostname");
        }
        return std::string(buf);
    }

    std::string addrToIP(const struct sockaddr_in &addr)
    {
        char buf[128] = "";
        if (ipAddrToDottedIP(&addr, buf, sizeof(buf)) == 0) {
            throw std::runtime_error("Failed to convert IOC address to IP string");
        }
        return std::string(buf);
    }
};