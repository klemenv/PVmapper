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

            /* Channel used to track when IOC goes down, 0 if IOC is unavailable */
            chid heartbeatId = 0;

            /* Mutex to protect access to any members of this struct */
            epicsMutex mutex;

            /* Pretty name for logging purposes */
            std::string prettyName;

            /** Populate IOC addr based on connected channel id */
            bool setAddr(chid chanId);
        };

        struct PvInfo {
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
        std::map<chid, std::shared_ptr<PvInfo>> m_searchedPvs;

        epicsMutex m_iocsMutex;
        epicsMutex m_pvsMutex;
        epicsMutex m_searchedPvsMutex;

        void handleConnectionStatus(struct connection_handler_args args);

    public:
        Directory();
        ~Directory();

        struct sockaddr_in findPv(const std::string& pvname);

        /** Remove PVs that noone has searched for in a while to optimize performance */
        void purgeCache(long maxAge=600);
};