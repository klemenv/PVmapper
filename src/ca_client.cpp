#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>

#include <cstdint>
#include <cstring>
#include <unistd.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

class SocketException : public std::exception {
    private:
        std::string m_msg;

    public:
        explicit SocketException(const std::string& message) {
            auto errno_ = errno;
            m_msg = message;
            auto pos = m_msg.find("{errno}");
            if (pos != std::string::npos) {
                m_msg = m_msg.replace(pos, 7, ::strerror(errno_));
            }
            fprintf(stderr, "%s\n", m_msg.c_str());
        }

        explicit SocketException(const char* message) {
            SocketException(std::string(message));
        }

        virtual ~SocketException() noexcept {};

        virtual const char* what() const noexcept {
            return m_msg.c_str();
        }
};

class AbstractProtocol {
    public:
        virtual std::vector<unsigned char> createSearchPacket(const std::vector<std::pair<uint32_t, std::string>> &pvs) = 0;
        virtual std::vector<std::pair<uint32_t, std::string>> parseSearchPackets(const std::vector<char> &buffer) = 0;
};

class ChannelAccess : public AbstractProtocol {
    private:
        static uint16_t const CMD_VERSION = 0x0;
        static uint16_t const CMD_SEARCH  = 0x6;

        struct Header {
            uint16_t command;
            uint16_t payloadLen;
            uint16_t dataType;
            uint16_t dataCount;
            uint32_t param1;
            uint32_t param2;
        };

    public:
        std::vector<unsigned char> createSearchPacket(const std::vector<std::pair<uint32_t, std::string>>& pvs) {
            std::vector<unsigned char> buffer(sizeof(Header));

            auto hdr = reinterpret_cast<Header *>(buffer.data());
            hdr->command = htons(CMD_VERSION);
            hdr->payloadLen = htons(0x0);
            hdr->dataType = htons(0x1);
            hdr->dataCount = htons(13);
            hdr->param1 = htons(0x0);
            hdr->param2 = htons(0x0);

            size_t offset = buffer.size();
            for (const auto& [chanId, pvname]: pvs) {
                size_t payloadLen = ((pvname.length() & 0xFFFF) + 7) & ~7;
                buffer.resize(buffer.size() + sizeof(Header) + payloadLen);
                hdr = reinterpret_cast<Header *>(buffer.data() + offset);
                hdr->command = htons(CMD_SEARCH);
                hdr->payloadLen = htons(payloadLen);
                hdr->dataType = htons(0x5);
                hdr->dataCount = htons(13);
                hdr->param1 = htonl(chanId);
                hdr->param2 = htonl(chanId);

                auto payload = reinterpret_cast<char *>(buffer.data() + offset + sizeof(Header));
                pvname.copy(payload, pvname.length() & 0xFFFF);

                offset += sizeof(Header) + payloadLen;
            }

            return buffer;
        }

        std::vector<std::pair<uint32_t, std::string>> parseSearchPackets(const std::vector<char>& buffer) {
            std::vector<std::pair<uint32_t, std::string>> pvs;
            size_t offset = 0;

            // Helper function to copy only non-null characters out from the buffer
            auto copyStr = [](const char* s, uint32_t len) {
                while (len > 0) {
                    if (s[len-1] != '\0') {
                        break;
                    }
                    len--;
                }
                return std::string(s, len);
            };

            while ((offset + sizeof(Header)) <= buffer.size()) {
                auto hdr = reinterpret_cast<const Header*>(buffer.data() + offset);
                auto payloadLen = ::ntohs(hdr->payloadLen);

                if (hdr->command == ::htons(CMD_SEARCH) && (offset + sizeof(Header) + payloadLen) <= buffer.size()) {
                    auto payload = reinterpret_cast<const char *>(buffer.data() + offset + sizeof(Header));
                    uint32_t chanId = ::ntohl(hdr->param1);
                    auto pv = copyStr(payload, payloadLen);
                    pvs.emplace_back(chanId, pv);
                }

                offset += sizeof(Header) + payloadLen;
            }

            return pvs;
        }
};

class ConnectionsManager {
    private:
        struct Connection {
            int sock = -1;
            struct sockaddr_in addr;
            std::shared_ptr<AbstractProtocol> protocol;
            operator bool() const {
                return (sock != -1);
            }
        };

        std::vector<Connection> m_listenConnections;
        std::vector<Connection> m_searchConnections;

    public:
        ~ConnectionsManager() {
            for (auto& conn: m_listenConnections) {
                ::close(conn.sock);
            }
            for (auto& conn: m_searchConnections) {
                ::close(conn.sock);
            }
        }

        void initSearchConnection(const std::string &ip, uint16_t port, const std::shared_ptr<AbstractProtocol>& protocol) {
            Connection conn;

            conn.protocol = protocol;

            conn.sock = ::socket(AF_INET, SOCK_DGRAM, 0);
            if (conn.sock < 0) {
                throw SocketException("failed to create socket - {errno}");
            }

            int enable = 1;
            if (::setsockopt(conn.sock, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(enable)) < 0) {
                throw SocketException("failed to enable broadcast on socket - {errno}");
            }

            conn.addr = {0}; // avoid using memset()
            conn.addr.sin_family = AF_INET;
            conn.addr.sin_port = ::htons(port);
            if (::inet_aton(ip.c_str(), reinterpret_cast<in_addr*>(&conn.addr.sin_addr.s_addr)) == 0) {
                throw SocketException("invalid IP address - {errno}");
            }

            m_searchConnections.emplace_back(conn);
        }

        void initListenConnection(const std::string& ip, uint16_t port, const std::shared_ptr<AbstractProtocol>& protocol) {
            Connection conn;

            conn.protocol = protocol;

            conn.sock = ::socket(AF_INET, SOCK_DGRAM, 0);
            if (conn.sock < 0) {
                throw SocketException("failed to create socket - {errno}");
            }
/*
            int optval = 1;
            if (::setsockopt(conn.sock, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval)) != 0) {
                throw SocketException("can't set reuse port option - {errno}");
            }
*/
            conn.addr = {0}; // avoid using memset()
            conn.addr.sin_family = AF_INET;
            conn.addr.sin_port = ::htons(port);
            if (ip.empty()) {
                conn.addr.sin_addr.s_addr = INADDR_ANY;
            } else if (::inet_aton(ip.c_str(), reinterpret_cast<in_addr*>(&conn.addr.sin_addr.s_addr)) == 0) {
                throw SocketException("invalid IP address - {errno}");
            }

            if (::bind(conn.sock, reinterpret_cast<sockaddr *>(&conn.addr), sizeof(conn.addr)) < 0 )  {
                throw SocketException("failed to bind to address - {errno}");
            }

            m_listenConnections.emplace_back(conn);
        }

        void sendSearchRequest(const std::vector<std::pair<uint32_t, std::string>>& pvs) {
            for (auto& conn: m_searchConnections) {
                auto msg = conn.protocol->createSearchPacket(pvs);
                ::sendto(conn.sock, msg.data(), msg.size(), 0, reinterpret_cast<sockaddr *>(&conn.addr), sizeof(sockaddr_in));
            }
        }

        void searchPvs(const std::vector<std::string>& pvnames) {
            std::vector<std::pair<uint32_t, std::string>> pvs;
            uint16_t cnt = 1;
            for (auto& pv: pvnames) {
                pvs.emplace_back(cnt++, pv);
            }
            sendSearchRequest(pvs);
        }

        void processSearchRequest(const Connection& conn) {
            char buffer[4096];
            struct sockaddr_in addr;
            socklen_t addrLen = sizeof(addr);
            auto recvd = ::recvfrom(conn.sock, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr *>(&addr), &addrLen);
            if (recvd > 0) {
                char ip[20] = {0};
                ::inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip)-1);

                printf("search request: %ld bytes\n", recvd);
                auto pvs = conn.protocol->parseSearchPackets({buffer, buffer + recvd});
                for (auto& pv: pvs) {
                    printf("* %s/%u: %s [%u]\n", ip, addr.sin_port, pv.second.c_str(), pv.first);
                    // TODO: call the handlers
                    // findPv(pv.second, pv.first, addr)
                }
            }
        }

        void processSearchResponse(const Connection& conn) {
            char buffer[4096];
            struct sockaddr_in addr;
            socklen_t addrLen = sizeof(addr);
            auto recvd = ::recvfrom(conn.sock, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr *>(&addr), &addrLen);
            if (recvd > 0) {
                printf("search response: %ld bytes\n", recvd);
                // TODO: call the handlers
            }
        }

        void process(double timeout=0.0) {
            size_t nFds = m_searchConnections.size() + m_listenConnections.size();
            auto fds = std::shared_ptr<pollfd[]>(new pollfd[nFds]);
            size_t cnt = 0;
            for (auto& conn: m_searchConnections) {
                fds[cnt].fd = conn.sock;
                fds[cnt].events = POLLIN & POLLERR;
                fds[cnt].revents = 0;
                cnt++;
            }
            for (auto& conn: m_listenConnections) {
                fds[cnt].fd = conn.sock;
                fds[cnt].events = POLLIN;
                fds[cnt].revents = 0;
                cnt++;
            }

            if (timeout <= 0) {
                timeout = 0.0;
            } else if (timeout < 0.001) {
                timeout = 0.001;
            }

            auto ret = ::poll(fds.get(), cnt, timeout*1000);
            if (ret > 0) {
                printf("sockets ready: %d\n", ret);

                // Helper function
                auto findConnection = [](const std::vector<Connection>& list, int sock) {
                    for (auto& conn: list) {
                        if (conn.sock == sock) {
                            return conn;
                        }
                    }
                    return Connection();
                };

                for (int i = 0; i < ret; i++) {
                    if (fds[i].revents & POLLIN) {
                        Connection conn;

                        conn = findConnection(m_listenConnections, fds[i].fd);
                        if (conn) {
                            processSearchRequest(conn);
                            continue;
                        }

                        conn = findConnection(m_searchConnections, fds[i].fd);
                        if (conn) {
                            processSearchResponse(conn);
                            continue;
                        }
                    }
                }
            }
        }
};

int main() {
    std::shared_ptr<ChannelAccess> caProto(new ChannelAccess);
    try {
        ConnectionsManager connMgr;
//        connMgr.initSearchConnection("192.168.1.255", 5064, caProto);
        connMgr.initListenConnection("192.168.1.48", 5053, caProto);
        connMgr.searchPvs({"TEST", "TEST2"});
        for (int i = 0; i < 100; i++) {
            connMgr.process(0.1);
        }
    } catch (std::exception& e) {
        fprintf(stderr, "%s\n", e.what());
        return 1;
    }
    return 0;
}