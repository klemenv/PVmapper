#pragma once

#include <sstream>

namespace Log {
    enum class Level {
        Debug,
        Verbose,
        Info,
        Error
    };
    void init(const std::string &id, const std::string &syslogFacility, Level lvl);

    Level getLogLevel();
    void setLogLevel(Level lvl);

    template<typename... Args>
    void write(Level lvl, const Args&... args)
    {
        std::ostringstream msg;
        write(lvl, msg, args...);
    }

    template<typename T, typename... Args>
    void write(Level lvl, std::ostringstream& msg, T value, const Args&... args)
    {
        msg << value;
        write(lvl, msg, args...);
    }

    void write(Level lvl, std::ostringstream &msg);
};

#define LOG_DEBUG(args ...)    Log::write(Log::Level::Debug,   args)
#define LOG_VERBOSE(args ...)  Log::write(Log::Level::Verbose, args)
#define LOG_INFO(args ...)     Log::write(Log::Level::Info,    args)
#define LOG_ERROR(args ...)    Log::write(Log::Level::Error,   args)
