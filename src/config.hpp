#pragma once

#include "logging.hpp"

#include <cstdint>
#include <regex>
#include <string>
#include <vector>

class AccessControl {
    public:
        enum Action
        {
            ALLOW,
            DENY
        };

        struct Entry {
            Action action;
            std::regex regex;
            std::string text;
        };

        std::vector<Entry> pvs;
        std::vector<Entry> clients;

        AccessControl() = default;
        AccessControl(const AccessControl &copy) = default;
};

class Config {
    public:
        typedef std::pair<std::string, uint16_t> Address;

        AccessControl           access_control;
        Log::Level              log_level = Log::Level::Error;
        std::string             syslog_facility; // leave empty to log to file
        std::string             syslog_id = "PVmapper";
        unsigned                purge_delay = 600;
        std::vector<Address>    ca_listen_addresses;
        std::vector<Address>    ca_search_addresses;

        void parseFile(const std::string &path);
};
