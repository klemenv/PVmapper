/**
 * @file dnscache.hpp
 * @brief Utilities for performant DNS resolution.
 */

#pragma once

#include <string>

/**
 * @class DnsCache
 * @brief Provides DNS reverse lookup caching.
 * 
 * Used primarily for performance purposes to manually resolve IP addresses to hostnames
 * to make log outputs more readable. After initial lookup, it caches results to avoid
 * stalling the application on repeated lookups.
 */
class DnsCache {
    public:
        /**
         * @brief Resolves an IP address to a hostname (if available in cache/resolvable).
         * 
         * @param ip The IP address string.
         * @return std::string The hostname if resolved, otherwise the original IP string.
         */
        static std::string resolveIP(const std::string& ip);
    private:
        static std::string _getHost(const std::string& ip);
        static unsigned _getTtl(const std::string& ip);
        static std::string _reverseIp(const std::string& ip);
};