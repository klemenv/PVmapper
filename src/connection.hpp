#pragma once

#include <stdexcept>
#include <string>

class SocketException : public std::exception {
    private:
        std::string m_msg;

    public:
        explicit SocketException(const std::string &message);
        explicit SocketException(const char *message) { SocketException(std::string(message)); };
        virtual ~SocketException() noexcept {};
        virtual const char *what() const noexcept { return m_msg.c_str(); }
};

class Connection {
    public:
        virtual void processIncoming() = 0;
};