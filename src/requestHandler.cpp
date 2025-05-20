#include "logging.hpp"
#include "requestHandler.hpp"
#include "util.hpp"

RequestHandler::RequestHandler(const AccessControl& accessControl, Directory& directory)
: caServer()
, m_accessControl(accessControl)
, m_directory(directory)
{}

pvExistReturn RequestHandler::pvExistTest(const casCtx& /* ctx */, const caNetAddr& client, const char* pvname_)
{
    std::string clientIP = Util::addrToHostName(client.getSockIP());
    std::string pvname(pvname_);

    LOG_DEBUG("Client ", clientIP, " searching for PV ", pvname);

    // Remove the optional .FIELD from PV names
    auto field = pvname.find_last_of('.');
    if (field != std::string::npos) {
        pvname.erase(field);
    }

    // Run the PV rules first
    for (auto& rule: m_accessControl.pvs) {
        // The first rule that matches will take place, either ALLOW or DENY
        if (regex_match(pvname, rule.regex)) {
            if (rule.action == AccessControl::ALLOW) {
                break;
            } else if (rule.action == AccessControl::DENY) {
                LOG_VERBOSE("Rejected request from ", clientIP, " searching for PV ", pvname, " due to '", rule.text, "' rule");
                return pverDoesNotExistHere;
            }
        }

        // Try next rule until there's any. If there's no `DENY PV *' rule
        // in the end, `ALLOW PV *' is assumed.
    }

    // Run rules for client IPs - CA only supports IPv4
    for (auto& rule: m_accessControl.clients) {
        // The first rule that matches will take place, either ALLOW or DENY
        if (regex_match(clientIP, rule.regex)) {
            if (rule.action == AccessControl::ALLOW) {
                break;
            } else if (rule.action == AccessControl::DENY) {
                LOG_VERBOSE("Rejected request from ", clientIP, " searching for PV ", pvname, " due to '", rule.text, "' rule");
                return pverDoesNotExistHere;
            }
        }

        // Try next rule until there's any. If there's no `DENY CLIENT *' rule
        // in the end, `ALLOW CLIENT *' is assumed.
    }

    try {
        auto iocAddr = m_directory.findPv(pvname, clientIP);
        auto iocname = Util::addrToHostName(iocAddr);
        LOG_INFO("Client ", clientIP, " searched for PV ", pvname, ", found on IOC ", iocname);
        return pvExistReturn(caNetAddr(iocAddr));
    } catch (Directory::SearchInProgress& e) {
        LOG_VERBOSE(e.what());
        return pverDoesNotExistHere;
    } catch (Directory::SearchError& e) {
        LOG_ERROR(e.what());
        return pverDoesNotExistHere;
    } catch (...) {
        return pverDoesNotExistHere;
    }
}

void RequestHandler::show(unsigned level) const
{
    caServer::show(level);
}
