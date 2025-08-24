#include "config.hpp"
#include "logging.hpp"
#include "dispatcher.hpp"

#include <csignal>
#include <getopt.h>

static Config config;

void usage(const std::string& prog)
{
    printf("Usage: %s [options] <config_file>\n", prog.c_str());
    printf("\n");
}

void restoreLogLevel(int)
{
    if (Log::getLogLevel() != config.log_level) {
        switch (config.log_level) {
        case Log::Level::Error:
            Log::write(Log::Level::Info, "Set logging level ERROR");
            break;
        case Log::Level::Info:
            Log::write(Log::Level::Info, "Set logging level INFO");
            break;
        case Log::Level::Verbose:
            Log::write(Log::Level::Info, "Set logging level VERBOSE");
            break;
        case Log::Level::Debug:
            Log::write(Log::Level::Info, "Set logging level DEBUG");
            break;
        }
        Log::setLogLevel(config.log_level);
    }
}

void increaseLogLevel(int)
{
    switch (Log::getLogLevel()) {
    case Log::Level::Error:
        Log::setLogLevel(Log::Level::Info);
        Log::write(Log::Level::Info, "Set logging level INFO");
        break;
    case Log::Level::Info:
        Log::setLogLevel(Log::Level::Verbose);
        Log::write(Log::Level::Info, "Set logging level VERBOSE");
        break;
    case Log::Level::Verbose:
        Log::write(Log::Level::Info, "Set logging level DEBUG");
        // fall-thru
    case Log::Level::Debug:
    default:
        Log::setLogLevel(Log::Level::Debug);
        break;
    }
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        usage(argv[0]);
        return 1;
    }
    config.parseFile(argv[1]);

    Log::init(config.syslog_id, config.syslog_facility, config.log_level);

    Dispatcher dispatcher(config);

    std::signal(SIGUSR1, restoreLogLevel);
    std::signal(SIGUSR2, increaseLogLevel);

    while (true) {
        dispatcher.run(0.1);
    }

    return 0;
}
