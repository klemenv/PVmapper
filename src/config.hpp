/**
 * @file config.hpp
 * @brief Configuration and access control definitions.
 */

#pragma once

#include "logging.hpp"

#include <cstdint>
#include <regex>
#include <string>
#include <vector>

/**
 * @class AccessControl
 * @brief Defines security policies for Clients and PVs.
 * 
 * Manages Access Control Lists (ACLs) to allow or deny access 
 * based on client IP addresses or PV names using regular expressions.
 */
class AccessControl {
    public:
        /**
         * @enum Action
         * @brief Defines the action to take when a rule matches.
         */
        enum Action
        {
            ALLOW, ///< Permit the operation.
            DENY   ///< Block the operation.
        };

        /**
         * @struct Entry
         * @brief Represents a single access control rule.
         */
        struct Entry {
            Action action;    ///< Action to take (ALLOW/DENY).
            std::regex regex; ///< Regular expression pattern to match against (IP or PV name).
            std::string text; ///< Original text representation of the regex for logging/debugging.
        };

        std::vector<Entry> pvs;     ///< List of rules applying to PV names.
        std::vector<Entry> clients; ///< List of rules applying to Client IP addresses.

        AccessControl() = default;
        AccessControl(const AccessControl &copy) = default;
};

/**
 * @class Config
 * @brief Application configuration container.
 * 
 * Stores all runtime configuration settings parsing from the config file,
 * including logging preferences, network headers, and access control rules.
 */
class Config {
    public:
        /**
         * @typedef Address
         * @brief Represents a network address as (IP String, Port).
         */
        typedef std::pair<std::string, uint16_t> Address;

        AccessControl           access_control;      ///< Security policy settings.
        Log::Level              log_level = Log::Level::Error; ///< Logging verbosity level.
        std::string             syslog_facility;     ///< Syslog facility name. If empty, logs to stdout/file.
        std::string             syslog_id = "PVmapper"; ///< Identity tag used in syslog messages.
        
        /**
         * @brief Intervals (in seconds) for exponential backoff of searches.
         * Defines how frequently we should retry searching for missing PVs.
         */
        std::vector<unsigned>   search_intervals = {1, 5, 10, 30, 60, 300};
        
        unsigned purge_delay = 600; ///< Time in seconds before purging an unreferenced PV from the search list.
        
        std::vector<Address>    ca_listen_addresses; ///< List of interfaces/ports to listen on for CA client requests.
        std::vector<Address>    ca_search_addresses; ///< List of destination addresses to forward CA searches to (IOCs).

        /**
         * @brief Parses configuration from a file.
         * 
         * Reads the specified configuration file and populates the members of this class.
         * Throws exceptions on parsing errors.
         * 
         * @param path The filesystem path to the configuration file.
         */
        void parseFile(const std::string &path);
};
