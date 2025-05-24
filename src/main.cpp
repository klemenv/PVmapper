#include "listener.hpp"
#include "logging.hpp"
//#include "requestHandler.hpp"
#include "directory.hpp"
#include "proto_ca.hpp"
#include "searcher.hpp"

#include <envDefs.h>
#include <fdManager.h>

#include <cctype>
#include <getopt.h>
#include <fstream>
#include <regex>

class Config {
	public:
		AccessControl accessControl;
		long purgeDelay = 600;
		std::string listenAddr = "";
		unsigned listenPort = 5053;

		Config(const std::string &configfile)
		{
			std::ifstream file(configfile);
			std::string line;

			std::regex reAllowPvs    ("^[ \t]*ALLOW_PV[= \t]+([^# ]*)[ \t]*(#.*)?$");
			std::regex reDenyPvs     ("^[ \t]*DENY_PV[= \t]+([^# ]*)[ \t]*(#.*)?$");
			std::regex reAllowClients("^[ \t]*ALLOW_CLIENT[= \t]+([^# ]*)[ \t]*(#.*)?$");
			std::regex reDenyClients ("^[ \t]*DENY_CLIENT[= \t]+([^# ]*)[ \t]*(#.*)?$");
			std::regex reEpics       ("^[ \t]*(EPICS_[A-Z_]*)[= \t]([^#]*)(#.*)?$");
			std::regex reLogLevel    ("^[ \t]*LOG_LEVEL[= \t]([^# \t]*)[ \t]*(#.*)?$");
			std::regex reLogFacility ("^[ \t]*LOG_FACILITY[= \t]([^# \t]*)[ \t]*(#.*)?$");
			std::regex reLogId       ("^[ \t]*LOG_ID[= \t]([^# \t]*)[ \t]*(#.*)?$");
			std::regex rePurgeDelay  ("^[ \t]*PURGE_DELAY[= \t]+([0-9]+)[ \t]*(#.*)?$");
			std::regex reListenAddr  ("^[ \t]*LISTEN_ADDR[= \t]+([^# ]*)[ \t]*(#.*)?$");
			std::regex reListenPort  ("^[ \t]*LISTEN_PORT[= \t]+([0-9]+)[ \t]*(#.*)?$");

			Log::Level logLevel = Log::Level::Error;
			std::string logFacility = "LOCAL0";
			std::string logId = "PVmapper";

			auto toLower = [](const std::string& s) {
				std::string o;
				for (auto c: s) {
					o += std::tolower(c);
				}
				return o;
			};

			while (std::getline(file, line))
			{
				std::smatch tokens;	
				if (std::regex_match(line, tokens, reAllowPvs)) {
					auto pattern = tokens[1].str();
					AccessControl::Entry entry = {AccessControl::ALLOW, std::regex(pattern), line};
					accessControl.pvs.emplace_back(entry);
				} else if (std::regex_match(line, tokens, reDenyPvs)) {
					auto pattern = tokens[1].str();
					AccessControl::Entry entry = {AccessControl::DENY, std::regex(pattern), line};
					accessControl.pvs.emplace_back(entry);
				} else if (std::regex_match(line, tokens, reAllowClients)) {
					auto pattern = tokens[1].str();
					AccessControl::Entry entry = {AccessControl::ALLOW, std::regex(pattern), line};
					accessControl.clients.emplace_back(entry);
				} else if (std::regex_match(line, tokens, reDenyClients)) {
					auto pattern = tokens[1].str();
					AccessControl::Entry entry = {AccessControl::DENY, std::regex(pattern), line};
					accessControl.clients.emplace_back(entry);
				} else if (std::regex_match(line, tokens, reEpics)) {
					epicsEnvSet(tokens[1].str().c_str(), tokens[2].str().c_str());

				} else if (std::regex_match(line, tokens, reLogLevel)) {
					if 		(toLower(tokens[1].str()) == "info") 	{ logLevel = Log::Level::Info; }
					else if (toLower(tokens[1].str()) == "verbose") { logLevel = Log::Level::Verbose; }
					else if (toLower(tokens[1].str()) == "debug") 	{ logLevel = Log::Level::Debug; }
					else if (toLower(tokens[1].str()) == "error") 	{ logLevel = Log::Level::Error; }
					else { fprintf(stderr, "ERROR: Invalid config value LOG_LEVEL=%s\n", tokens[1].str().c_str()); }

				} else if (std::regex_match(line, tokens, reLogFacility)) {
					logFacility = tokens[1].str();

				} else if (std::regex_match(line, tokens, reLogId)) {
					logId = tokens[1].str();

				} else if (std::regex_match(line, tokens, rePurgeDelay)) {
					auto tmp = std::atol(tokens[1].str().c_str());
					if (tmp > 0) { purgeDelay = tmp; }
					else { fprintf(stderr, "ERROR: Invalid config value PURGE_DELAY=%s\n", tokens[1].str().c_str()); }
				
				} else if (std::regex_match(line, tokens, reListenAddr)) {
					listenAddr = tokens[1].str();

				} else if (std::regex_match(line, tokens, reListenAddr)) {
					listenPort = std::atol(tokens[1].str().c_str());
				}
			}

			Log::init(logId, logFacility, logLevel);
		}
};

void usage(const std::string& prog)
{
	printf("Usage: %s [options] <config_file>\n", prog.c_str());
	printf("\n");
}

int main(int argc, char** argv)
{
	AccessControl accessControl;
	Directory directory;

	if (argc != 2) {
		usage(argv[0]);
		return 1;
	}

	auto config = Config(argv[1]);
/*
	RequestHandler requests(accessControl, directory);

	epicsTime lastPurge = epicsTime::getCurrent();
	while (true) {
		fileDescriptorManager.process(1.0);
		ca_pend_event(.001);

		// Time to purge the cache?
		if (lastPurge + purgeDelay < epicsTime::getCurrent()) {
			directory.purgeCache(purgeDelay);
			lastPurge = epicsTime::getCurrent();
		}
	}
*/

    std::shared_ptr<ChannelAccess> caProto(new ChannelAccess);
    try {
		Searcher searcher("192.168.1.255", 5064, caProto, [](auto pvname, auto iocIp, auto iocPort, auto pkt) {
			printf("Found %s on %s:%u\n", pvname.c_str(), iocIp.c_str(), iocPort);
		});
		searcher.searchPVs({"TEST", "TEST2"});
		sleep(1);
		searcher.processIncoming();

		Listener listener(config.listenAddr,
						  config.listenPort,
						  config.accessControl,
						  caProto,
						  [](auto pvname, auto clientIp, auto clientPort) -> std::vector<unsigned char> {
			printf("%s:%u is searching for %s\n", clientIp.c_str(), clientPort, pvname.c_str());
			return std::vector<unsigned char>();
		});
		sleep(5);
		listener.processIncoming();
/*
		ConnectionsManager connMgr;
        connMgr.initSearchConnection("192.168.1.255", 5064, caProto);
        connMgr.initListenConnection("", 5053, caProto);
		Listener listener("", 5053, accessControl, caProto);
		connMgr.searchPvs({"TEST", "TEST2"});
		for (int i = 0; i < 100; i++) {
            connMgr.process(0.1);
        }
			*/
    } catch (std::exception& e) {
        fprintf(stderr, "%s\n", e.what());
        return 1;
    }

	return 0;
}
