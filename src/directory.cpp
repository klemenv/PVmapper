#include "directory.hpp"
#include "logging.hpp"
#include "util.hpp"

#include <stdexcept>

Directory::Directory()
{
    // Initialize CA library for callbacks operation
    ca_context_create(ca_enable_preemptive_callback);
}

Directory::~Directory()
{
}

struct sockaddr_in Directory::findPv(const std::string& pvname, const std::string& client)
{
    std::shared_ptr<PvInfo> pvinfo;

    m_pvsMutex.lock();
    auto pvIter = m_pvs.find(pvname);
    if (pvIter == m_pvs.end()) {
        // This is the first time anyone requested the PV
        pvinfo.reset(new PvInfo());
        pvinfo->name = pvname;
        pvinfo->lastSearched = epicsTime::getCurrent();
        m_pvs[pvname] = pvinfo;
        m_pvsMutex.unlock();

    } else {
        // We found the PV in cache, let's check if the IOC is alive
        pvinfo = pvIter->second;
        m_pvsMutex.unlock();

        pvinfo->mutex.lock();
        pvinfo->lastSearched = epicsTime::getCurrent();
        auto ioc = pvinfo->ioc;
        pvinfo->mutex.unlock();

        if (!ioc) {
            throw std::runtime_error(pvname + " not in cache, search in progress");
        }

        ioc->mutex.lock();
        bool iocActive = (ioc->status == IocInfo::Status::ACTIVE);
        struct sockaddr_in addr = ioc->addr;
        auto iocname = ioc->name;
        ioc->mutex.unlock();

        if (iocActive) {
            return addr;
        }

        // The IOC must has shut down, continue & restart the search but only once
        pvinfo->mutex.lock();
        pvinfo->ioc.reset();
        pvinfo->mutex.unlock();
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
        throw std::runtime_error(pvname + " not in cache, search failed");
    }

    m_connectedPvsMutex.lock();
    m_connectedPvs[chanId] = pvinfo;
    m_connectedPvsMutex.unlock();

    throw std::runtime_error(pvname + " not in cache, starting the search");
}

void Directory::purgeCache(long maxAge)
{
    std::vector<std::string> pvs;
    unsigned searching = 0;

    LOG_DEBUG("Purging PVs that have been disconnected for more than %ld seconds\n", maxAge);

    // Remove non-existing PVs that no-one has searched for in a while
    m_connectedPvsMutex.lock();
    for (auto it=m_connectedPvs.cbegin(); it!=m_connectedPvs.cend(); ) {
        if ((it->second->lastSearched + maxAge) < epicsTime::getCurrent()) {
            pvs.push_back(it->second->name);
            it = m_connectedPvs.erase(it);
        } else {
            it++;
            if (!it->second->ioc) {
                searching++;
            }
        }
    }
    m_connectedPvsMutex.unlock();

    m_pvsMutex.lock();
    for (auto& pv: pvs) {
        m_pvs.erase(pv);
    }
    unsigned cached = m_pvs.size();
    m_pvsMutex.unlock();

    m_iocsMutex.lock();
    for (auto it = m_iocs.cbegin(); it != m_iocs.cend(); ) {
        if (it->second.use_count() == 1) {
            it = m_iocs.erase(it);
        } else {
            it++;
        }
    }
    unsigned iocs = m_iocs.size();
    m_iocsMutex.unlock();

    unsigned purged = pvs.size();
    LOG_INFO("Purged %u uninterested PVs, %u PVs remain in cache, searching for %u PVs, %u IOCs", purged, cached, searching, iocs);
}

void Directory::handleConnectionStatus(struct connection_handler_args args)
{
    std::string iocname = "";
    // If PV has an alias, the original name might differ from ca_name()
    std::string pvname = ca_name(args.chid);

    if (args.op == CA_OP_CONN_UP) {
        bool keepConnected = false;
        std::string iocIP = "";

        // We'll need IOC addr info later in the locked section
        struct sockaddr_in addr;
        try {
            addr = Util::chIdToAddr(args.chid);
            iocIP = Util::addrToIP(addr);
            iocname = Util::addrToHostName(addr);
        } catch (...) {}

        // Find or initialize IocInfo
        m_iocsMutex.lock();
        auto ioc = m_iocs[iocIP];
        if (!iocIP.empty() && !ioc) {
            ioc.reset(new IocInfo());
            ioc->status = Directory::IocInfo::Status::ACTIVE;
            ioc->addr = addr;
            ioc->name = iocname;
            // Keep the PV connected so that we get notified if IOC shuts down
            ioc->heartbeatId = args.chid;
            m_iocs[iocIP] = ioc;
            keepConnected = true;
        } else {
            // We only need one PV from same IOC to stay connected
            ca_clear_channel(args.chid);
        }
        m_iocsMutex.unlock();

        std::shared_ptr<PvInfo> pvinfo;
        m_connectedPvsMutex.lock();
        auto it = m_connectedPvs.find(args.chid);
        if (it != m_connectedPvs.end()) {
            pvinfo = it->second;
            if (!keepConnected) {
                m_connectedPvs.erase(it);
            }
        }
        m_connectedPvsMutex.unlock();

        if (pvinfo) {
            pvinfo->mutex.lock();
            pvinfo->ioc = ioc;
            pvname = pvinfo->name; // we may be using an alias, which could differ from ca_name()
            pvinfo->mutex.unlock();
        }

        LOG_DEBUG("%s found on IOC %s, storing in cache\n", pvname.c_str(), iocIP.c_str());

    } else if (args.op == CA_OP_CONN_DOWN) {
        // This should only trigger for the PVs monitoring the IOC status.
        // All the others we've already disconnected right away.
        // But just in case, we'll check in m_connectedPvs
        m_connectedPvsMutex.lock();
        auto it = m_connectedPvs.find(args.chid);
        std::shared_ptr<IocInfo> ioc;
        if (it != m_connectedPvs.end()) {
            ioc = it->second->ioc;
            pvname = it->second->name;
        }
        m_connectedPvsMutex.unlock();

        if (ioc) {
            // Since this is a shared_ptr used by any PV from same IOC,
            // update it so that other PVs can detect that the IOC is gone.
            ioc->mutex.lock();
            iocname = ioc->name;
            ioc->status = Directory::IocInfo::Status::UNAVAILABLE;
            ioc->heartbeatId = 0;
            ioc->mutex.unlock();
        }

        // Remove from active PVs, if any
        m_pvsMutex.lock();
        m_pvs.erase(pvname);
        m_pvsMutex.unlock();

        // The same PV might not be present in the same IOC after it comes back
        // online, so just disconnect and forget about it
        ca_clear_channel(args.chid);

        LOG_DEBUG("%s disconnected, IOC %s probably shut down, removed from cache\n", pvname.c_str(), iocname.c_str());
    }
}
