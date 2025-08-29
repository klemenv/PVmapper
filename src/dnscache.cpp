#include "dnscache.hpp"

#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netdb.h>
#ifdef USE_LIB_RESOLVE
#include <resolv.h>
#endif

#include <chrono>
#include <map>
#include <vector>

struct Entry {
    std::string host;
    std::chrono::steady_clock::time_point expires;
};

static std::map<std::string, Entry> g_cache;

std::string DnsCache::resolveIP(const std::string &ip)
{
    auto now = std::chrono::steady_clock::now();
    auto it = g_cache.find(ip);
    if (it != g_cache.end()) {
        if (now <= it->second.expires) {
            return it->second.host;
        } else {
            g_cache.erase(it);
        }
    }

    auto host = _getHost(ip);
    auto expires = now + std::chrono::seconds(_getTtl(ip));
    if (host.empty()) {
        host = ip;
    }
    g_cache[ip] = { host, expires };

    return host;
}

std::string DnsCache::_getHost(const std::string& ip)
{
    char host[128] = {0};
    struct sockaddr_in sa;
    if (::inet_pton(AF_INET, ip.c_str(), &(sa.sin_addr)) != 1) {
        return "";
    }

    sa.sin_family = AF_INET;
    sa.sin_port = 0;

    auto res = ::getnameinfo(reinterpret_cast<struct sockaddr *>(&sa), sizeof(sa),
                             host, sizeof(host), NULL, 0, NI_NAMEREQD);
    if (res != 0) {
        return "";
    }

    return host;
}

std::string DnsCache::_reverseIp(const std::string& ip)
{
    std::string delimiter = ".";
    std::vector<std::string> tokens;
    size_t pos = 0;
    std::string token;
    std::string s = ip;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        tokens.push_back(token);
        s.erase(0, pos + 1);
    }
    tokens.push_back(s);

    s = "";
    for (auto it = tokens.rbegin(); it != tokens.rend(); it++) {
        s += *it + delimiter;
    }
    s += "in-addr.arpa";

    return s;
}

unsigned DnsCache::_getTtl(const std::string& ip)
{
    unsigned ttl = 86400;

#ifdef USE_LIB_RESOLVE
    // TODO: This only tested for IPv4 addresses
    auto reverseQuery = _reverseIp(ip);
    unsigned char nsbuf[NS_MAXMSG];
    auto len = res_query(reverseQuery.c_str(), ns_c_in, ns_t_ptr, nsbuf, sizeof(nsbuf));
    if (len >= 0) {
        ns_msg msg;
        ns_rr rr;
        if (ns_initparse(nsbuf, len, &msg) >= 0) {
            auto count = ns_msg_count(msg, ns_s_an);
            for (int i = 0; i < count; ++i) {
                if (ns_parserr(&msg, ns_s_an, i, &rr) >= 0 && ns_rr_type(rr) == ns_t_ptr) {
                    ttl = ns_rr_ttl(rr);
                    if (ttl < 600) {
                        ttl = 600;
                    }
                    break;
                }
            }
        }
    }
#endif

    return ttl;
}