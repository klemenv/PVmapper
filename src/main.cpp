#include "config.hpp"
#include "logging.hpp"
#include "dispatcher.hpp"

#include <getopt.h>

void usage(const std::string& prog)
{
    printf("Usage: %s [options] <config_file>\n", prog.c_str());
    printf("\n");
}

int main(int argc, char** argv)
{
    if (argc != 2) {
        usage(argv[0]);
        return 1;
    }
    Config config;
    config.parseFile(argv[1]);

    Log::init(config.syslog_id, config.syslog_facility, config.log_level);

    Dispatcher dispatcher(config);

    while (true) {
        dispatcher.run(0.1);
    }

    return 0;
}
