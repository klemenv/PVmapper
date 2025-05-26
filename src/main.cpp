#include "config.hpp"
#include "listener.hpp"
#include "logging.hpp"
#include "directory.hpp"
#include "proto_ca.hpp"
#include "searcher.hpp"
#include "iocguard.hpp"
#include "connmgr.hpp"

#include <cctype>
#include <getopt.h>

void usage(const std::string& prog)
{
    printf("Usage: %s [options] <config_file>\n", prog.c_str());
    printf("\n");
}

int main(int argc, char** argv)
{
    Directory directory;
    std::vector<std::shared_ptr<Searcher>> searchersCA;
    std::map<std::pair<std::string, uint16_t>, std::shared_ptr<IocGuard>> iocs;

    if (argc != 2) {
        usage(argv[0]);
        return 1;
    }
    Config config;
    config.parseFile(argv[1]);

    Log::init(config.syslog_id, config.syslog_facility, config.log_level);

    std::shared_ptr<ChannelAccess> caProto(new ChannelAccess);
    try {
        IocGuard::DisconnectCb iocDisconnectCb = [&directory, &iocs](auto iocIP, auto iocPort) {
            // TODO: directory.iocDisconnected()
            auto it = iocs.find(std::make_pair(iocIP, iocPort));
            if (it != iocs.end()) {
                iocs.erase(it);
                ConnectionsManager::remove(it->second);
            }
        };

        Searcher::FoundPvCb foundPvCaCb = [&directory, &iocDisconnectCb, &caProto, &iocs](auto pvname, auto iocIP, auto iocPort, auto response) {
            printf("foundPvCaCb(%s, %s, %u)\n", pvname.c_str(), iocIP.c_str(), iocPort);
            try {
                // Try to find existing IOC, .at() will throw if not found
                auto iocGuard = iocs.at(std::make_pair(iocIP, iocPort));
                directory.foundPv(pvname, iocGuard, response);
                return;
            } catch (...) {}
            try {
                // Need to create a new IocGuard
                auto iocGuard = std::shared_ptr<IocGuard>(new IocGuard(iocIP, iocPort, caProto, iocDisconnectCb));
                iocs[std::make_pair(iocIP, iocPort)] = iocGuard;
                ConnectionsManager::add(iocGuard);
                directory.foundPv(pvname, iocGuard, response);
            } catch (...) {}
        };
        auto searcher = std::shared_ptr<Searcher>(
            new Searcher("192.168.1.255",
                         5064,
                         caProto,
                         foundPvCaCb)
        );
        ConnectionsManager::add(searcher);
        searchersCA.emplace_back(searcher);

        Listener::SearchPvCb searchPvCaCb = [&directory, &searchersCA](auto pvname, auto clientIP, auto clientPort) -> std::vector<unsigned char> {
            printf("searchPvCaCb(%s, %s, %u)\n", pvname.c_str(), clientIP.c_str(), clientPort);
            auto response = directory.findPv(pvname);
            if (response.empty()) {
                for (auto& searcher: searchersCA) {
                    searcher->searchPVs({pvname});
                }
            }
            return response;
        };
        auto listener = std::shared_ptr<Listener>(
            new Listener(config.listen_addresses[0].first,
                         config.listen_addresses[0].second,
                         config.access_control,
                         caProto,
                         searchPvCaCb)
        );
        ConnectionsManager::add(listener);

    } catch (SocketException& e) {
        fprintf(stderr, "%s\n", e.what());
        return 1;
    }

    while (true) {
        ConnectionsManager::run(0.1);
        // TODO:purge cache
    }

    return 0;
}
