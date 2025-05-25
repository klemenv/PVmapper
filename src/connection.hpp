#pragma once

#include <arpa/inet.h>

#include <stdexcept>
#include <string>

class SocketException : public std::runtime_error {
    private:
        std::string m_msg;

    public:
        explicit SocketException(const std::string &message, int err = 0);
        explicit SocketException(const char *message, int err = 0)
            : SocketException(std::string(message), err) {};
        virtual ~SocketException() noexcept {};
        virtual const char *what() const noexcept { return m_msg.c_str(); }
};

class Connection {
    protected:
        int m_sock = -1;
        struct sockaddr_in m_addr;

    public:
        virtual ~Connection() = 0;
        int getSocket() { return m_sock; };
        virtual bool isConnected() { return (m_sock != -1); };
        virtual void processIncoming() {};
        virtual void processOutgoing() {};
};