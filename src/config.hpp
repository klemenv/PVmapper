#pragma once

#include "logging.hpp"

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
        typedef std::pair<std::string, unsigned> Address;

        AccessControl           access_control;
        Log::Level              log_level = Log::Level::Error;
        std::string             syslog_facility; // leave empty to log to file
        std::string             syslog_id = "PVmapper";
        unsigned                purge_delay = 600;
        std::vector<Address>    listen_addresses;

        void parseFile(const std::string &path);
};
