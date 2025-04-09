#include "logging.hpp"
#include "requestHandler.hpp"

#include <envDefs.h>
#include <fdManager.h>

#include <cctype>
#include <getopt.h>
#include <fstream>
#include <regex>

void parseConfig(const std::string& configfile, AccessControl& accessControl, long& purgeDelay)
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
	std::regex rePurgeDelay  ("^[ \t]*PURGE_DELAY[= \t]([0-9]+)[ \t]*(#.*)?$");

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
		}
	}

	Log::init(logId, logFacility, logLevel);
}

void usage(const std::string& prog)
{
	printf("Usage: %s [options] <config_file>\n", prog.c_str());
	printf("\n");
}

int main(int argc, char** argv)
{
	AccessControl accessControl;
	Directory directory;
	long purgeDelay = 600;

	if (argc != 2) {
		usage(argv[0]);
		return 1;
	}

	parseConfig(argv[1], accessControl, purgeDelay);

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

	return 0;
}
