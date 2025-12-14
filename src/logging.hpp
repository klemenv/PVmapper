/**
 * @file logging.hpp
 * @brief Simple logging facility with syslog support.
 */

#pragma once

#include <sstream>

/**
 * @namespace Log
 * @brief Namespace containing logging utilities and state.
 */
namespace Log {
    /**
     * @enum Level
     * @brief Defines severity levels for log messages.
     */
    enum class Level {
        Debug,   ///< Detailed debug information.
        Verbose, ///< Verbose operational info.
        Info,    ///< Standard informational messages.
        Error    ///< Critical errors.
    };

    /**
     * @brief Initializes the logging subsystem.
     * 
     * @param id The identifier string (tag) for syslog messages.
     * @param syslogFacility The syslog facility to use (e.g., "local0"). If empty, logs to stderr.
     * @param lvl The initial logging verbosity level.
     */
    void init(const std::string &id, const std::string &syslogFacility, Level lvl);

    /**
     * @brief Gets the current log level.
     * @return Level Current globally set log level.
     */
    Level getLogLevel();

    /**
     * @brief Sets the global log level.
     * @param lvl New log level.
     */
    void setLogLevel(Level lvl);

    /**
     * @brief Variadic template entry point for writing log messages.
     * 
     * Constructs a message stream and passes it to the recursive writer.
     * 
     * @tparam Args Argument types to log.
     * @param lvl Severity level of this message.
     * @param args The values to append to the log message.
     */
    template<typename... Args>
    void write(Level lvl, const Args&... args)
    {
        std::ostringstream msg;
        write(lvl, msg, args...);
    }

    /**
     * @brief Recursive helper to unroll variadic arguments into the stream.
     * 
     * @tparam T Type of the current value being written.
     * @tparam Args Remaining argument types.
     * @param lvl Severity level.
     * @param msg The accumulating string stream.
     * @param value The current value to write.
     * @param args Remaining values.
     */
    template<typename T, typename... Args>
    void write(Level lvl, std::ostringstream& msg, T value, const Args&... args)
    {
        msg << value;
        write(lvl, msg, args...);
    }

    /**
     * @brief Base case for the recursive writer.
     * 
     * Writes the final composed message to the configured output (syslog or stderr)
     * if the severity is sufficient.
     * 
     * @param lvl Severity level.
     * @param msg The fully constructed message stream.
     */
    void write(Level lvl, std::ostringstream &msg);
};

/** @brief Helper macro for Debug logs */
#define LOG_DEBUG(args ...)    Log::write(Log::Level::Debug,   args)
/** @brief Helper macro for Verbose logs */
#define LOG_VERBOSE(args ...)  Log::write(Log::Level::Verbose, args)
/** @brief Helper macro for Info logs */
#define LOG_INFO(args ...)     Log::write(Log::Level::Info,    args)
/** @brief Helper macro for Error logs */
#define LOG_ERROR(args ...)    Log::write(Log::Level::Error,   args)
