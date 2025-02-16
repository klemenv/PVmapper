#include "directory.hpp"
#include "logging.hpp"

#include <stdexcept>

Directory::Directory()
{
    // Initialize CA library for callbacks operation
    ca_context_create(ca_enable_preemptive_callback);
}

Directory::~Directory()
{
}

struct sockaddr_in Directory::findPv(const std::string& pvname)
{
    std::shared_ptr<PvInfo> pvinfo;

    m_pvsMutex.lock();
    auto pvIter = m_pvs.find(pvname);
    if (pvIter != m_pvs.end()) {
        // We found the PV in cache, let's check if the IOC is alive
        pvinfo = pvIter->second;
        m_pvsMutex.unlock();

        pvinfo->mutex.lock();
        pvinfo->lastSearched = epicsTime::getCurrent();
        auto ioc = pvinfo->ioc;
        pvinfo->mutex.unlock();

        if (!ioc) {
            throw std::runtime_error("PV '" + pvname + "' not in directory, search in progress");
        }

        ioc->mutex.lock();
        bool active = (ioc->status == IocInfo::Status::ACTIVE);
        struct sockaddr_in addr = ioc->addr;
        auto hostname = ioc->prettyName;
        ioc->mutex.unlock();

        if (active) {
//            LOG_INFO("PV '%s' searched by '%s' found on IOC '%s:%hu'\n", pvname.c_str(), clientIP.c_str(), ioc);
            LOG_INFO("PV '%s' found on IOC '%s'\n", pvname.c_str(), hostname);
            return addr;
        }

        // The IOC must has shut down, restart the search but only once
        pvinfo->mutex.lock();
        pvinfo->ioc.reset();
        pvinfo->mutex.unlock();
    } else {
        // This is the first time anyone requested the PV
        pvinfo.reset(new PvInfo());
        pvinfo->lastSearched = epicsTime::getCurrent();
        m_pvs[pvname] = pvinfo;
        m_pvsMutex.unlock();
    }

    // Start searching for the PV
    chid chanId;
    auto status = ca_create_channel(
        pvname.c_str(),
        [](struct connection_handler_args args) {
            static_cast<Directory *>(ca_puser(args.chid))->handleConnectionStatus(args);
        },
        this,
        10,
        &chanId
    );
    if (status != ECA_NORMAL) {
        throw std::runtime_error("PV '" + pvname + "' not in directory, search failed");
    }

    m_searchedPvsMutex.lock();
    m_searchedPvs[chanId] = pvinfo;
    m_searchedPvsMutex.unlock();

    throw std::runtime_error("PV '" + pvname + "' not in directory, starting the search");
}

void Directory::purgeCache(long maxAge)
{
    unsigned removed = 0;
    unsigned remain = 0;

    LOG_DEBUG("Flushing PVs that have been disconnected for more than %ld seconds", maxAge);

    // Remove non-existing PVs that no-one has searched for in a while
    m_searchedPvsMutex.lock();
    for (auto it=m_searchedPvs.cbegin(); it!=m_searchedPvs.cend(); ) {
        if ((it->second->lastSearched + maxAge) < epicsTime::getCurrent()) {
            it = m_searchedPvs.erase(it);
            removed++;
        } else {
            it++;
            remain++;
        }
    }
    m_searchedPvsMutex.unlock();

    LOG_DEBUG("Removed %u PVs not searched for over %ld seconds, %u remain in cache", removed, maxAge, remain);
}

void Directory::handleConnectionStatus(struct connection_handler_args args)
{
    char hostname[512];
    ca_get_host_name(args.chid, hostname, sizeof(hostname));

    if (args.op == CA_OP_CONN_UP) {
        // We'll need IOC addr info later in the locked section
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        aToIPAddr(hostname, 5064, &addr);

        // Find or initialize IocInfo
        m_iocsMutex.lock();
        auto ioc = m_iocs[hostname];
        if (!ioc) {
            ioc.reset(new IocInfo());
            m_iocs[hostname] = ioc;
        }
        m_iocsMutex.unlock();

        // Flag IOC as active and assign the heartbeatPV if needed
        ioc->mutex.lock();
        ioc->status = Directory::IocInfo::Status::ACTIVE;
        ioc->addr = addr;
        ioc->prettyName = hostname;
        if (ioc->heartbeatId == 0) {
            // Keep the PV connected so that we get notified if IOC shuts down
            ioc->heartbeatId = args.chid;
        } else {
            // We only need one PV from same IOC to stay connected
            ca_clear_channel(args.chid);
        }
        ioc->mutex.unlock();

        std::shared_ptr<PvInfo> pvinfo;
        m_searchedPvsMutex.lock();
        auto it = m_searchedPvs.find(args.chid);
        if (it != m_searchedPvs.end()) {
            pvinfo = it->second;
            m_searchedPvs.erase(it);
        }
        m_searchedPvsMutex.unlock();

        if (pvinfo) {
            pvinfo->mutex.lock();
            pvinfo->ioc = ioc;
            pvinfo->mutex.unlock();
        }

    } else if (args.op == CA_OP_CONN_DOWN) {
        // This will only trigger for the PVs monitoring the IOC status.
        // All the others we've already disconnected right away

        std::shared_ptr<IocInfo> ioc;
        m_iocsMutex.lock();
        auto it = m_iocs.find(hostname);
        if (it != m_iocs.end()) {
            ioc = it->second;
        }
        m_iocsMutex.unlock();

        if (ioc) {
            ioc->mutex.lock();
            if (ioc->heartbeatId == args.chid) {
                ioc->status = Directory::IocInfo::Status::UNAVAILABLE;
                ioc->heartbeatId = 0;
            }
            ioc->mutex.unlock();
        }

        // CA says they may invoke disconnected state before PV even connects
        m_searchedPvsMutex.lock();
        m_searchedPvs.erase(args.chid);
        m_searchedPvsMutex.unlock();

        // The same PV might not be present in the same IOC after it comes back
        // online, so just disconnect and forget about it
        ca_clear_channel(args.chid);
    }
}