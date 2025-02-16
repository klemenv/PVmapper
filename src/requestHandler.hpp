#pragma once

#include "directory.hpp"

#include <casdef.h>

#include <regex>
#include <vector>

struct AccessControl {
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

class RequestHandler : public caServer {
    private:
        AccessControl m_accessControl;
        Directory& m_directory;

    public:
        RequestHandler(const AccessControl& accessControl, Directory& directory);
        pvExistReturn pvExistTest(const casCtx & /* ctx */, const caNetAddr& client, const char* pvname) override;
        void show(unsigned level) const;
};