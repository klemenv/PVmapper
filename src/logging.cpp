#include "logging.hpp"

#include <chrono>
#include <iostream>
#include <syslog.h>

namespace Log {
    static Level _level = Level::Error;
    static bool _syslog = false;

    inline int level2prio(Level level) {
        switch (level) {
            case Level::Info:       return LOG_INFO;
            case Level::Verbose:    return LOG_NOTICE;
            case Level::Debug:      return LOG_DEBUG;
            default:                return LOG_ERR;
        }
    }

    inline const char* level2str(Level level) {
        switch (level) {
            case Level::Info:       return "INFO";
            case Level::Verbose:    return "VERBOSE";
            case Level::Debug:      return "DEBUG";
            default:                return "ERROR";
        }
    }

    void init(const std::string& id, const std::string& syslogFacility, Level lvl)
    {
        _level = lvl;

        if (syslogFacility.empty() == false) {
            int facility = LOG_LOCAL0;
            if        (syslogFacility == "LOCAL1") {
                facility = LOG_LOCAL1;
            } else if (syslogFacility == "LOCAL2") {
                facility = LOG_LOCAL2;
            } else if (syslogFacility == "LOCAL3") {
                facility = LOG_LOCAL3;
            } else if (syslogFacility == "LOCAL4") {
                facility = LOG_LOCAL4;
            } else if (syslogFacility == "LOCAL5") {
                facility = LOG_LOCAL5;
            } else if (syslogFacility == "LOCAL6") {
                facility = LOG_LOCAL6;
            } else if (syslogFacility == "LOCAL7") {
                facility = LOG_LOCAL7;
            } else if (syslogFacility == "USER") {
                facility = LOG_USER;
            } else if (syslogFacility == "SYSLOG") {
                facility = LOG_SYSLOG;
            } else if (syslogFacility == "DAEMON") {
                facility = LOG_DAEMON;
            }
            static char ident[80] = {0};
            id.copy(ident, sizeof(ident - 1));
            openlog(ident, LOG_CONS, facility);
            _syslog = true;
        }
    };

    Level getLogLevel()
    {
        return _level;
    }

    void setLogLevel(Level lvl)
    {
        _level = lvl;
    }

    void write(Level lvl, std::ostringstream &msg)
    {
        if (lvl >= _level) {
            if (_syslog == true) {
                syslog(level2prio(lvl), "%s", msg.str().c_str());
            } else {
                const auto now = std::chrono::system_clock::now();
                auto millis = (now.time_since_epoch().count() / 1000000) % 1000;
                const std::time_t now_t = std::chrono::system_clock::to_time_t(now);
                auto timeinfo = std::localtime(&now_t);
                char buffer[64] = {0};
                strftime(buffer, sizeof(buffer) - 1, "%Y-%m-%d %H:%M:%S", timeinfo);
                printf("%s:%03d %s: %s\n", buffer, (int)millis, level2str(lvl), msg.str().c_str());
            }
        }
    }

};