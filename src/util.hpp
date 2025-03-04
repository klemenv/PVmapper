#pragma once

#include <cadef.h>
#include <osiSock.h>

#include <string>

namespace Util {

    /** Return sockaddr_in of the IOC serving chanId, or throw runtime_error */
    struct sockaddr_in chIdToAddr(chid chanId);
    std::string addrToHostName(const struct sockaddr_in& addr);
    std::string addrToIP(const struct sockaddr_in &addr);
};