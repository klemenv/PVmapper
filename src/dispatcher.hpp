#pragma once

#include "proto_ca.hpp"
#include "iocguard.hpp"
#include "listener.hpp"
#include "searcher.hpp"

#include <memory>

class Dispatcher {
    private:
        typedef std::pair<std::string, uint16_t> Address;

        struct PvInfo {
            /* Timestamp when last client searched for this PV, used by book-keeping */
            std::chrono::steady_clock::time_point lastSearched;

            /* Pointer to the IOC structure (may be empty) to determine IOC status.
             * This is shared by all PVs residing on the same IOC and allows for quick
             * determination if the PV is valid or not. When IOC disconnects, its
             * status will change.
            */
            std::shared_ptr<IocGuard> ioc;

            /* Raw packet response from the IOC, returned to the client */
            std::vector<unsigned char> response;
        };

        enum class Proto {
            CHANNEL_ACCESS,
        };

        const Config& m_config;
        std::chrono::steady_clock::time_point m_lastPurge;
        std::shared_ptr<ChannelAccess> m_caProto;
        std::map<Address, std::shared_ptr<IocGuard>> m_iocs;
        std::vector<std::shared_ptr<Searcher>> m_caSearchers;
        std::vector<std::shared_ptr<Listener>> m_caListeners;
        std::map<std::string, PvInfo> m_connectedPVs;

        void addListener(const std::string& ip, uint16_t port, Proto proto);
        void addSearcher(const std::string& ip, uint16_t port, Proto proto);

        void iocDisconnected(const std::string& iocIP, uint16_t port);
        void caPvFound(const std::string& pvname, const std::string& iocIP, uint16_t iocPort, const std::vector<unsigned char>& response);
        std::vector<unsigned char> caPvSearched(const std::string &pvname, const std::string &clientIP, uint16_t clientPort);

    public:
        Dispatcher(const Config& config);
        void run(double timeout = 0.1);
};