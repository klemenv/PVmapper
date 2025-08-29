#pragma once

#include <string>

class DnsCache {
    public:
        static std::string resolveIP(const std::string& ip);
    private:
        static std::string _getHost(const std::string& ip);
        static unsigned _getTtl(const std::string& ip);
        static std::string _reverseIp(const std::string& ip);
};