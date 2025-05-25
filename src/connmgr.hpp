#pragma once

#include "connection.hpp"

#include <memory>
#include <vector>

class ConnectionsManager {
    private:
        std::vector<std::shared_ptr<Connection>> m_connections;

    public:
        static void add(const std::shared_ptr<Connection>& connection);
        static void remove(const std::shared_ptr<Connection>& connection);
        static void run(double timeout = 0.1);
};