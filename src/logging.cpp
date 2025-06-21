#include "logging.hpp"

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

    void write(Level lvl, std::ostringstream &msg)
    {
        if (lvl >= _level) {
            if (_syslog == true) {
                syslog(level2prio(lvl), "%s", msg.str().c_str());
            } else {
                printf("%s: %s\n", level2str(lvl), msg.str().c_str());
            }
        }
    }

};