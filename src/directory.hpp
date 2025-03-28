#pragma once

#include <cadef.h>
#include <casdef.h>
#include <epicsMutex.h>
#include <epicsThread.h>
#include <epicsTime.h>

#include <atomic>
#include <map>
#include <memory>
#include <string>
#include <vector>

class Directory {
    private:
        struct IocInfo {
            /* IOC status */
            enum class Status
            {
                UNAVAILABLE, //!< Initial state or when IOC shuts down
                ACTIVE,      //!< IOC is active as detected by heartbeat PV
            } status = Status::UNAVAILABLE;

            /* IOC address to be returned to clients */
            struct sockaddr_in addr;

            /* Mutex to protect access to any members of this struct */
            epicsMutex mutex;

            /* Pretty name for logging purposes */
            std::string name;
        };

        struct PvInfo {
            /* Original name that was searched for, it may be different than real PV name if aliased  */
            std::string name;

            /* Timestamp when last client searched for this PV, used by book-keeping */
            epicsTime lastSearched;

            /* Pointer to the IOC structure (may be empty) to determine IOC status.
             * This is shared by all PVs residing on the same IOC and allows for quick
             * determination if the PV is valid or not. When IOC disconnects, its
             * status will change.
            */
            std::shared_ptr<IocInfo> ioc;

            /* Mutex to protect access to any members of this struct */
            epicsMutex mutex;
        };

        std::map<std::string, std::shared_ptr<IocInfo>> m_iocs;
        std::map<std::string, std::shared_ptr<PvInfo>> m_pvs;
        std::map<chid, std::string> m_chan2pvname;

        epicsMutex m_iocsMutex;
        epicsMutex m_pvsMutex;  // guards m_pvs ans m_chan2pvname

        void handleConnectionStatus(struct connection_handler_args args);

        std::shared_ptr<PvInfo> find(const std::string& pvname);
        std::shared_ptr<PvInfo> find(chid caChanId);
        void insert(const std::string &pvname, chid caChanId, const std::shared_ptr<PvInfo>& pvinfo);
        void erase(const std::string& pvname);
        void erase(chid caChanId);
        std::vector<std::string> getPvNames();

    public:
        class SearchError : public std::runtime_error
        {
            public:
                SearchError(const std::string& what) : std::runtime_error(what) {}
        };
        class SearchInProgress : public std::runtime_error
        {
            public:
                SearchInProgress(const std::string& what) : std::runtime_error(what) {}
        };

        Directory();
        ~Directory();

        struct sockaddr_in findPv(const std::string& pvname, const std::string& client);

        /** Remove PVs that noone has searched for in a while to optimize performance */
        void purgeCache(long maxAge=600);
};
