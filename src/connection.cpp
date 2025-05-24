#include "connection.hpp"

#include <cstring>

SocketException::SocketException(const std::string& message) {
    auto errno_ = errno;
    m_msg = message;
    auto pos = m_msg.find("{errno}");
    if (pos != std::string::npos) {
        m_msg = m_msg.replace(pos, 7, ::strerror(errno_));
    }
}
