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

void Directory::insert(const std::string &pvname, chid caChanId, const std::shared_ptr<PvInfo>& pvinfo)
{
    m_chan2pvname[caChanId] = pvname;
    m_pvs[pvname] = pvinfo;
}

std::shared_ptr<Directory::PvInfo> Directory::find(const std::string& pvname)
{
    std::shared_ptr<PvInfo> pvinfo;
    auto it = m_pvs.find(pvname);
    if (it != m_pvs.end()) {
        pvinfo = it->second;
    }
    return pvinfo;
}

std::shared_ptr<Directory::PvInfo> Directory::find(chid caChanId)
{
    std::shared_ptr<PvInfo> pvinfo;
    auto it = m_chan2pvname.find(caChanId);
    if (it != m_chan2pvname.end()) {
        auto jt = m_pvs.find(it->second);
        if (jt != m_pvs.end()) {
            pvinfo = jt->second;
        }
    }
    return pvinfo;
}

void Directory::erase(const std::string& pvname)
{
    m_pvs.erase(pvname);
    for (auto const& it: m_chan2pvname) {
        if (it.second == pvname) {
            m_chan2pvname.erase(it.first);
            break;
        }
    }
}

void Directory::erase(chid caChanId)
{
    auto it = m_chan2pvname.find(caChanId);
    if (it != m_chan2pvname.end()) {
        m_pvs.erase(it->second);
        m_chan2pvname.erase(it);
    }
}

std::vector<std::string> Directory::getPvNames()
{
    std::vector<std::string> pvnames;
    for (auto const& it: m_pvs) {
        pvnames.push_back(it.first);
    }
    return pvnames;
}

struct sockaddr_in Directory::findPv(const std::string& pvname, const std::string& client)
{
    m_pvsMutex.lock();
    auto pvinfo = find(pvname);
    m_pvsMutex.unlock();

    if (pvinfo) {
        struct sockaddr_in addr;
        bool iocActive = false;

        // We found the PV in cache, let's check if the IOC is alive
        pvinfo->mutex.lock();
        pvinfo->lastSearched = epicsTime::getCurrent();
        if (pvinfo->ioc) {
            pvinfo->ioc->mutex.lock();
            iocActive = (pvinfo->ioc->status == IocInfo::Status::ACTIVE);
            addr = pvinfo->ioc->addr;
            pvinfo->ioc->mutex.unlock();

            if (!iocActive) {
                // IOC has shut down, new IOC will be created when channel reconnects
                pvinfo->ioc.reset();
            }
        }
        pvinfo->mutex.unlock();

        if (iocActive) {
            return addr;
        }

        throw SearchInProgress(pvname + " not in cache, search in progress");
    }

    pvinfo.reset(new PvInfo());
    pvinfo->name = pvname;
    pvinfo->lastSearched = epicsTime::getCurrent();

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
        throw SearchError(pvname + " not in cache, search failed");
    }

    m_pvsMutex.lock();
    insert(pvname, chanId, pvinfo);
    m_pvsMutex.unlock();

    throw SearchInProgress(pvname + " not in cache, starting the search");
}

void Directory::purgeCache(long maxAge)
{
    std::vector<std::string> pvs;
    unsigned searching = 0;
    unsigned purged = 0;

    LOG_DEBUG("Purging PVs that have been disconnected for more than ", maxAge, " seconds");

    m_pvsMutex.lock();
    auto pvnames = getPvNames();
    m_pvsMutex.unlock();

    for (auto pvname: pvnames) {
        m_pvsMutex.lock();
        auto pvinfo = find(pvname);
        m_pvsMutex.unlock();

        if (pvinfo) {
            pvinfo->mutex.lock();
            bool expired = (pvinfo->lastSearched + maxAge) < epicsTime::getCurrent();
            auto ioc = pvinfo->ioc;
            pvinfo->mutex.unlock();

            bool iocActive = false;
            if (ioc) {
                // This is the only place where we keep pvinfo and ioc locked at the same time
                ioc->mutex.lock();
                iocActive = (pvinfo->ioc->status == IocInfo::Status::ACTIVE);
                ioc->mutex.unlock();
            }

            if (!iocActive) {
                if (expired) {
                    m_pvsMutex.lock();
                    m_pvs.erase(pvname);
                    m_pvsMutex.unlock();

                    purged++;
                } else {
                    searching++;
                }
            }
        }
    }
    unsigned cached = pvnames.size() - purged;

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

    LOG_INFO("Purged ", purged, " uninterested PVs, ", cached, " PVs remain in cache, searching for ", searching, " PVs, ", iocs, " IOCs");
}

void Directory::handleConnectionStatus(struct connection_handler_args args)
{
    std::string iocname = "";
    // If PV has an alias, the original name might differ from ca_name()
    std::string pvname = ca_name(args.chid);

    if (args.op == CA_OP_CONN_UP) {
        bool disconnect = true;
        std::string iocIP = "";
        std::shared_ptr<IocInfo> ioc;

        // We'll need IOC addr info later in the locked section
        struct sockaddr_in addr;
        try {
            addr = Util::chIdToAddr(args.chid);
            iocIP = Util::addrToIP(addr);
            iocname = Util::addrToHostName(addr);
        } catch (...) {}

        if (!iocIP.empty()) {
            // Find or initialize IocInfo
            m_iocsMutex.lock();
            ioc = m_iocs[iocIP];
            if (!ioc) {
                ioc.reset(new IocInfo());
                ioc->status = Directory::IocInfo::Status::ACTIVE;
                ioc->addr = addr;
                ioc->name = iocname;
                m_iocs[iocIP] = ioc;
                // Keep the PV connected so that we get notified if IOC shuts down
                disconnect = false;
            }
            m_iocsMutex.unlock();
        }

        m_pvsMutex.lock();
        auto pvinfo = find(args.chid);
        m_pvsMutex.unlock();

        if (pvinfo) {
            pvinfo->mutex.lock();
            pvinfo->ioc = ioc;
            pvname = pvinfo->name; // we may be using an alias, which could differ from ca_name()
            pvinfo->mutex.unlock();
        }

        if (disconnect) {
            // We only need one PV from same IOC to stay connected
            ca_clear_channel(args.chid);
            LOG_VERBOSE("PV ", pvname, " found on IOC ", iocname, ", storing in cache");
        } else {
            LOG_VERBOSE("PV ", pvname, " found on IOC ", iocname, ", storing in cache and keep connected");
        }

    } else if (args.op == CA_OP_CONN_DOWN) {
        // This should only trigger for the PVs monitoring the IOC status.
        // All the others we've already disconnected right away.
        m_pvsMutex.lock();
        auto pvinfo = find(args.chid);
        erase(args.chid);
        m_pvsMutex.unlock();

        if (pvinfo) {
            pvinfo->mutex.lock();
            auto ioc = pvinfo->ioc;
            pvname = pvinfo->name;
            pvinfo->mutex.unlock();

            if (ioc) {
                // Since this is a shared_ptr used by any PV from same IOC,
                // update it so that other PVs can detect that the IOC is gone.
                ioc->mutex.lock();
                iocname = ioc->name;
                ioc->status = Directory::IocInfo::Status::UNAVAILABLE;
                ioc->mutex.unlock();

                // Prevent future PV searches to reuse this entry
                m_iocsMutex.lock();
                for (auto it = m_iocs.cbegin(); it != m_iocs.cend(); it++) {
                    if (it->second->name == iocname) {
                        m_iocs.erase(it);
                        break;
                    }
                }
                m_iocsMutex.unlock();
            }
        }

        // The PV might not be present in the same IOC after it comes back
        // online, so just disconnect and forget about it
        ca_clear_channel(args.chid);

        LOG_VERBOSE("PV ", pvname, " disconnected, IOC ", iocname, " probably shut down, removed from cache");
    }
}
