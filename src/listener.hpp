#pragma once

#include "config.hpp"
#include "proto.hpp"
#include "connection.hpp"

#include <functional>
#include <memory>
#include <regex>
#include <string>
#include <vector>

class Listener : public Connection {
    public:
        typedef std::function<Protocol::Bytes (const std::string & /*pvname*/, const std::string & /*client IP*/, uint16_t /*client port*/)> PvSearchedCb;

    private:
        const AccessControl& m_accessControl;
        std::shared_ptr<Protocol> m_protocol;
        PvSearchedCb m_searchPvCb;

        bool checkAccessControl(const std::string &pvname, const std::string &client, uint16_t port);

    public:
   
        Listener(const std::string& ip, uint16_t port, const AccessControl& accessControl, const std::shared_ptr<Protocol>& protocol, PvSearchedCb& cb);
        void processIncoming();
};