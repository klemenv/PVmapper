#include "config.hpp"

#include <fstream>

void Config::parseFile(const std::string& path)
{
    std::regex reAllowPvs    ("^[ \t]*ALLOW_PV[= \t]+([^# ]*)[ \t]*(#.*)?$");
    std::regex reDenyPvs     ("^[ \t]*DENY_PV[= \t]+([^# ]*)[ \t]*(#.*)?$");
    std::regex reAllowClients("^[ \t]*ALLOW_CLIENT[= \t]+([^# ]*)[ \t]*(#.*)?$");
    std::regex reDenyClients ("^[ \t]*DENY_CLIENT[= \t]+([^# ]*)[ \t]*(#.*)?$");
    std::regex reLogLevel    ("^[ \t]*LOG_LEVEL[= \t]([^# \t]*)[ \t]*(#.*)?$");
    std::regex reLogFacility ("^[ \t]*SYSLOG_FACILITY[= \t]([^# \t]*)[ \t]*(#.*)?$");
    std::regex reLogId       ("^[ \t]*SYSLOG_ID[= \t]([^# \t]*)[ \t]*(#.*)?$");
    std::regex reSearchInt   ("^[ \t]*SEARCH_INTERVAL[= \t]+([0-9]+)[ \t]*(#.*)?$");
    std::regex rePurgeDelay  ("^[ \t]*PURGE_DELAY[= \t]+([0-9]+)[ \t]*(#.*)?$");
    std::regex reCaListenAddr("^[ \t]*CA_LISTEN_ADDRESS[= \t]+([0-9]{1,3}(\\.[0-9]{1,3}){3})(:([0-9]{1,5}))?");
    std::regex reCaSearchAddr("^[ \t]*CA_SEARCH_ADDRESS[= \t]+([0-9]{1,3}(\\.[0-9]{1,3}){3})(:([0-9]{1,5}))?");

    auto toLower = [](const std::string& s) {
        std::string o;
        for (auto c: s) {
            o += static_cast<char>(std::tolower(c));
        }
        return o;
    };

    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        std::smatch tokens;

        // Strip off any comments
        auto pos = line.find_first_of('#');
        if (pos != std::string::npos) {
            line = line.erase(pos);
        }
        pos = line.find_last_not_of(' ');
        if (pos != std::string::npos) {
            line = line.erase(pos + 1);
        }

        if (std::regex_match(line, tokens, reAllowPvs)) {
            auto pattern = tokens[1].str();
            AccessControl::Entry entry = {AccessControl::ALLOW, std::regex(pattern), line};
            access_control.pvs.emplace_back(entry);
        } else if (std::regex_match(line, tokens, reDenyPvs)) {
            auto pattern = tokens[1].str();
            AccessControl::Entry entry = {AccessControl::DENY, std::regex(pattern), line};
            access_control.pvs.emplace_back(entry);
        } else if (std::regex_match(line, tokens, reAllowClients)) {
            auto pattern = tokens[1].str();
            AccessControl::Entry entry = {AccessControl::ALLOW, std::regex(pattern), line};
            access_control.clients.emplace_back(entry);
        } else if (std::regex_match(line, tokens, reDenyClients)) {
            auto pattern = tokens[1].str();
            AccessControl::Entry entry = {AccessControl::DENY, std::regex(pattern), line};
            access_control.clients.emplace_back(entry);

        } else if (std::regex_match(line, tokens, reLogLevel)) {
            if      (toLower(tokens[1].str()) == "info")    { log_level = Log::Level::Info; }
            else if (toLower(tokens[1].str()) == "verbose") { log_level = Log::Level::Verbose; }
            else if (toLower(tokens[1].str()) == "debug")   { log_level = Log::Level::Debug; }
            else if (toLower(tokens[1].str()) == "error")   { log_level = Log::Level::Error; }
            else { fprintf(stderr, "ERROR: Invalid config value LOG_LEVEL=%s\n", tokens[1].str().c_str()); }

        } else if (std::regex_match(line, tokens, reLogFacility)) {
            syslog_facility = tokens[1].str();

        } else if (std::regex_match(line, tokens, reLogId)) {
            syslog_id = tokens[1].str();

        } else if (std::regex_match(line, tokens, reSearchInt)) {
            auto tmp = std::atol(tokens[1].str().c_str());
            if (tmp > 0) { search_interval = static_cast<unsigned>(tmp); }
            else { fprintf(stderr, "ERROR: Invalid config value SEARCH_INTERVAL=%s\n", tokens[1].str().c_str()); }

        } else if (std::regex_match(line, tokens, rePurgeDelay)) {
            auto tmp = std::atol(tokens[1].str().c_str());
            if (tmp > 0) { purge_delay = static_cast<unsigned>(tmp); }
            else { fprintf(stderr, "ERROR: Invalid config value PURGE_DELAY=%s\n", tokens[1].str().c_str()); }

        } else if (std::regex_match(line, tokens, reCaListenAddr)) {
            auto addr = tokens[1].str();
            auto tmp = std::atol(tokens[4].str().c_str());
            if (tmp > 0 && tmp < 65535) {
                ca_listen_addresses.emplace_back(addr, tmp);
            }

        } else if (std::regex_match(line, tokens, reCaSearchAddr)) {
            auto addr = tokens[1].str();
            auto tmp = std::atol(tokens[4].str().c_str());
            if (tmp > 0 && tmp < 65535) {
                ca_search_addresses.emplace_back(addr, tmp);
            }

        }
    }

    if (ca_listen_addresses.empty()) {
        ca_listen_addresses.emplace_back("0.0.0.0", 5053);
    }
}
