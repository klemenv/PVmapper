#pragma once

#include "proto.hpp"
#include "connection.hpp"

#include <arpa/inet.h>

#include <memory>
#include <regex>
#include <string>
#include <vector>

class AccessControl {
    public:
        enum Action
        {
            ALLOW,
            DENY
        };

        struct Entry {
            Action action;
            std::regex regex;
            std::string text;
        };

        std::vector<Entry> pvs;
        std::vector<Entry> clients;

        AccessControl() = default;
        AccessControl(const AccessControl &copy) = default;
};

class Listener : public Connection {
    public:
        typedef std::vector<unsigned char>(*SearchPvCb)(const std::string& /*pvname*/, const std::string& /*client IP*/, uint16_t /*client port*/);

    private:
        int m_sock = -1;
        struct sockaddr_in m_addr;
        const AccessControl& m_accessControl;
        std::shared_ptr<AbstractProtocol> m_protocol;
        SearchPvCb m_searchPvCb;

        bool checkAccessControl(const std::string &pvname, const std::string &client);

    public:
   
        Listener(const std::string& ip, uint16_t port, const AccessControl& accessControl, const std::shared_ptr<AbstractProtocol>& protocol, SearchPvCb cb);
        void processIncoming();
};