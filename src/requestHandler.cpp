#include "logging.hpp"
#include "requestHandler.hpp"

RequestHandler::RequestHandler(const AccessControl& accessControl, Directory& directory)
: caServer()
, m_accessControl(accessControl)
, m_directory(directory)
{}

pvExistReturn RequestHandler::pvExistTest(const casCtx& /* ctx */, const caNetAddr& client, const char* pvname_)
{
    std::string clientIP( inet_ntoa(client.getSockIP().sin_addr) );
    std::string pvname(pvname_);

    LOG_VERBOSE("Client '%s' searching for PV '%s'\n", clientIP.c_str(), pvname.c_str());

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
                LOG_DEBUG("Rejected request from '%s' searching for PV '%s' due to '%s' rule\n", clientIP.c_str(), pvname.c_str(), rule.text.c_str());
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
                LOG_DEBUG("Rejected request from '%s' searching for PV '%s' due to '%s' rule\n", clientIP.c_str(), pvname.c_str(), rule.text.c_str());
                return pverDoesNotExistHere;
            }
        }

        // Try next rule until there's any. If there's no `DENY CLIENT *' rule
        // in the end, `ALLOW CLIENT *' is assumed.
    }

    try {
        auto iocAddr = m_directory.findPv(pvname, clientIP);
        return pvExistReturn(caNetAddr(iocAddr));
    } catch (std::runtime_error& e) {
        LOG_INFO("%s\n", e.what());
        return pverDoesNotExistHere;
    }
}

void RequestHandler::show(unsigned level) const
{
    caServer::show(level);
}