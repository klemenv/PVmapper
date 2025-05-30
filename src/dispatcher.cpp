#include "dispatcher.hpp"
#include "connmgr.hpp"

Dispatcher::Dispatcher(const Config& config)
    : m_config(config)
    , m_lastPurge(std::chrono::steady_clock::now())
    , m_caProto(new ChannelAccess)
{
    for (auto& addr: config.ca_listen_addresses) {
        try {
            addListener(addr.first, addr.second, Dispatcher::Proto::CHANNEL_ACCESS);
        } catch (SocketException& e) {
            fprintf(stderr, "Failed to initilize Listener(%s, %u): %s\n", addr.first.c_str(), addr.second, e.what());
            return;
        }
    }

    for (auto& addr: config.ca_search_addresses) {
        try {
            addSearcher(addr.first, addr.second, Dispatcher::Proto::CHANNEL_ACCESS);
        } catch (SocketException& e) {
            fprintf(stderr, "Failed to initilize Searcher(%s, %u): %s\n", addr.first.c_str(), addr.second, e.what());
            return;
        }
    }
}

void Dispatcher::iocDisconnected(const std::string& iocIP, uint16_t iocPort)
{
    auto it = m_iocs.find(std::make_pair(iocIP, iocPort));
    if (it != m_iocs.end()) {
        m_iocs.erase(it);
        ConnectionsManager::remove(it->second);
    }
}

void Dispatcher::caPvFound(const std::string& pvname, const std::string& iocIP, uint16_t iocPort, const std::vector<unsigned char>& response)
{
    printf("foundPvCaCb(%s, %s, %u)\n", pvname.c_str(), iocIP.c_str(), iocPort);
    std::shared_ptr<IocGuard> iocGuard;
    auto it = m_iocs.find(std::make_pair(iocIP, iocPort));
    if (it != m_iocs.end()) {
        iocGuard = it->second;
    } else {
        using namespace std::placeholders;
        IocGuard::DisconnectCb disconnectCb = std::bind(&Dispatcher::iocDisconnected, this, _1, _2);
        try {
            iocGuard.reset(new IocGuard(iocIP, iocPort, m_caProto, disconnectCb));
        } catch (...) {
            return;
        }
        m_iocs[std::make_pair(iocIP, iocPort)] = iocGuard;
        ConnectionsManager::add(iocGuard);
    }

    auto& pv = m_connectedPVs[pvname];
    pv.ioc = iocGuard;
    pv.response = response;
    pv.lastSearched = std::chrono::steady_clock::now();
}

std::vector<unsigned char> Dispatcher::caPvSearched(const std::string &pvname, const std::string &clientIP, uint16_t clientPort)
{
    try {
        auto pv = m_connectedPVs.at(pvname);
        if (pv.ioc && pv.ioc->isConnected()) {
            LOG_INFO("Client ", clientIP, ":", clientPort, " searched for ", pvname, ", found in cache");
            return pv.response;
        }
        // The IOC must got disconnected
        m_connectedPVs.erase(pvname);
    } catch (std::out_of_range&) {}

    LOG_INFO("Client ", clientIP, ":", clientPort, " searched for ", pvname, ", not in cache, starting the search");
    for (auto& searcher: m_caSearchers) {
        searcher->addPV(pvname);
    }
    return std::vector<unsigned char>();
}

void Dispatcher::addListener(const std::string& ip, uint16_t port, Dispatcher::Proto proto)
{
    using namespace std::placeholders;
    std::shared_ptr<Listener> listener;

    if (proto == Proto::CHANNEL_ACCESS) {
        Listener::PvSearchedCb pvSearchedPv = std::bind(&Dispatcher::caPvSearched, this, _1, _2, _3);
        listener.reset(new Listener(ip, port, m_config.access_control, m_caProto, pvSearchedPv));
    }
    if (listener) {
        m_caListeners.emplace_back(listener);
        ConnectionsManager::add(listener);
    }
}

void Dispatcher::addSearcher(const std::string& ip, uint16_t port, Dispatcher::Proto proto)
{
    using namespace std::placeholders;
    std::shared_ptr<Searcher> searcher;

    if (proto == Proto::CHANNEL_ACCESS) {
        Searcher::PvFoundCb pvFoundCb = std::bind(&Dispatcher::caPvFound, this, _1, _2, _3, _4);
        searcher.reset(new Searcher(ip, port, m_caProto, pvFoundCb));
    }
    if (searcher) {
        m_caSearchers.emplace_back(searcher);
        ConnectionsManager::add(searcher);
    }
}

void Dispatcher::run(double timeout)
{
    ConnectionsManager::run(timeout);
    auto diff = (std::chrono::steady_clock::now() - m_lastPurge);
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(diff).count();
    if (duration > m_config.purge_delay) {
        // TODO: searchers purge
    }
}