/**
 * @file connection.hpp
 * @brief Base connection and exception classes for network communication.
 */

#pragma once

#include <arpa/inet.h>

#include <stdexcept>
#include <string>

/**
 * @class SocketException
 * @brief Exception thrown when socket operations fail.
 * 
 * Provides a standard way to report socket errors, optionally including
 * the system errno.
 */
class SocketException : public std::runtime_error {
    private:
        std::string m_msg;

    public:
        /**
         * @brief Constructs a SocketException.
         * @param message Descriptive error message.
         * @param err Optional system error code (errno).
         */
        explicit SocketException(const std::string &message, int err = 0);

        /**
         * @brief Constructs a SocketException from a C-string.
         * @param message Descriptive error message.
         * @param err Optional system error code (errno).
         */
        explicit SocketException(const char *message, int err = 0)
            : SocketException(std::string(message), err) {};

        virtual ~SocketException() noexcept {};

        /**
         * @brief Returns the error description.
         * @return const char* The error message.
         */
        virtual const char *what() const noexcept { return m_msg.c_str(); }
};

/**
 * @class Connection
 * @brief Abstract base class representing a network connection.
 * 
 * Manages the underlying socket file descriptor and provides an interface for
 * processing incoming and outgoing data.
 */
class Connection {
    protected:
        int m_sock = -1;            ///< The underlying socket file descriptor.
        struct sockaddr_in m_addr;  ///< Address structure for the connection.

    public:
        /**
         * @brief Virtual destructor.
         */
        virtual ~Connection() = 0;

        /**
         * @brief Gets the socket file descriptor.
         * @return int The socket FD, or -1 if invalid.
         */
        int getSocket() { return m_sock; };

        /**
         * @brief Checks if the connection is active.
         * @return bool True if the socket is valid (!= -1).
         */
        virtual bool isConnected() { return (m_sock != -1); };

        /**
         * @brief Process pending incoming data.
         * Default implementation does nothing.
         */
        virtual void processIncoming() {};

        /**
         * @brief Process pending outgoing data.
         * Default implementation does nothing.
         */
        virtual void processOutgoing() {};
};