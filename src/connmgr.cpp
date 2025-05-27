#include "connmgr.hpp"

#include <poll.h>

static ConnectionsManager g_connmgr;

void ConnectionsManager::add(const std::shared_ptr<Connection>& connection)
{
    g_connmgr.m_connections.emplace_back(connection);
}

void ConnectionsManager::remove(const std::shared_ptr<Connection>& connection)
{
    for (auto it = g_connmgr.m_connections.begin(); it != g_connmgr.m_connections.end();) {
        if (connection->getSocket() == (*it)->getSocket()) {
            it = g_connmgr.m_connections.erase(it);
        } else {
            it++;
        }
    }
}

void ConnectionsManager::run(double timeout) {
    size_t nFds = g_connmgr.m_connections.size();
    auto fds = std::shared_ptr<pollfd[]>(new pollfd[nFds]);
    for (size_t i = 0; i < g_connmgr.m_connections.size(); i++) {
        fds[i].fd = g_connmgr.m_connections[i]->getSocket();
        fds[i].events = POLLIN;
        fds[i].revents = 0;
    }

    if (timeout <= 0) {
        timeout = 0.0;
    } else if (timeout < 0.001) {
        timeout = 0.001;
    }

    // Use poll to process all connections with incoming packets
    if (::poll(fds.get(), nFds, timeout*1000) > 0) {
        for (size_t i = 0; i < nFds; i++) {
            if (fds[i].revents & POLLIN) {
                g_connmgr.m_connections[i]->processIncoming();
            }
        }
    }

    // Trigger each connection to send out any packets
    // And drop closed connections
    for (auto it = g_connmgr.m_connections.begin(); it != g_connmgr.m_connections.end();) {
        (*it)->processOutgoing();

        if ((*it)->isConnected() == false) {
            it = g_connmgr.m_connections.erase(it);
        } else {
            it++;
        }
    }
}
