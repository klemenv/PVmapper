#include "connection.hpp"

#include <cstring>

SocketException::SocketException(const std::string &message, int err)
    : std::runtime_error(message)
    , m_msg(message)
{
    if (err == 0) {
        err = errno;
    }
    if (err != 0) {
        m_msg += " - ";
        m_msg += strerror(err);
    }
}

Connection::~Connection() = default;