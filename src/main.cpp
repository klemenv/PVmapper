#include "logging.hpp"
#include "requestHandler.hpp"

#include <envDefs.h>
#include <fdManager.h>

#include <getopt.h>
#include <fstream>
#include <regex>

void parseConfig(const std::string& configfile, AccessControl& accessControl)
{
	std::ifstream file(configfile);
	std::string line;

	std::regex reAllowPvs    ("^[ \t]*ALLOW[ \t]+PV[ \t]+([^#]*)(#.*)?$");
	std::regex reDenyPvs     ("^[ \t]*DENY[ \t]+PV[ \t]+([^#]*)(#.*)?$");
	std::regex reAllowClients("^[ \t]*ALLOW[ \t]+CLIENT[ \t]+([^#]*)(#.*)?$");
	std::regex reDenyClients ("^[ \t]*DENY[ \t]+CLIENT[ \t]+([^#]*)(#.*)?$");
	std::regex reEpics       ("^[ \t]*(EPICS_[A-Z_]*)[= \t]([^#]*)(#.*)?$");

	while (std::getline(file, line)) {
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
		}
	}
}

int main(int argc, char** argv)
{
	AccessControl accessControl;
	Directory directory;
	long purgeTime = 600;
	errlogSevEnum loglevel = errlogMajor;

	int c;
	while ((c = getopt(argc, argv, "c:vp:l:")) != -1) {
		switch (c) {
		case 'v':
			// print version
			break;
		case 'c':
			// config file
			parseConfig(optarg, accessControl);
			break;
		case 'p':
			// purge time
			purgeTime = std::atoi(optarg);
			break;
		case 'l':
			// log level
			switch (std::atoi(optarg)) {
				case 0:  loglevel = errlogInfo;  break;
				case 1:  loglevel = errlogMinor; break;
				case 2:  loglevel = errlogMajor; break;
				default: loglevel = errlogFatal; break;
			}
			break;
		default:
			break;
		}
	}

	errlogSetSevToLog(loglevel);
	RequestHandler requests(accessControl, directory);

	epicsTime lastPurge = epicsTime::getCurrent();
	while (true) {
		fileDescriptorManager.process(1.0);
		ca_pend_event(.001);

		// Time to purge the cache?
		if (lastPurge + purgeTime < epicsTime::getCurrent()) {
			directory.purgeCache(purgeTime);
		}
	}

	return 0;
}
