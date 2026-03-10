#pragma once

#include "dbase/net/inet_address.h"
#include "dbase/net/socket_ops.h"

#include <utility>

namespace dbase::net
{
class Socket
{
    public:
        Socket() noexcept = default;
        explicit Socket(SocketType sock) noexcept;
        ~Socket();

        Socket(const Socket&) = delete;
        Socket& operator=(const Socket&) = delete;

        Socket(Socket&& other) noexcept;
        Socket& operator=(Socket&& other) noexcept;

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] SocketType fd() const noexcept;

        [[nodiscard]] SocketType release() noexcept;
        void reset(SocketType sock = kInvalidSocket) noexcept;

        void bindAddress(const InetAddress& addr);
        void listen(int backlog = SOMAXCONN);
        [[nodiscard]] Socket accept(InetAddress* peerAddr);

        void shutdownWrite();

        [[nodiscard]] InetAddress localAddress() const;
        [[nodiscard]] InetAddress peerAddress() const;

        void setReuseAddr(bool on);
        void setReusePort(bool on);
        void setTcpNoDelay(bool on);
        void setKeepAlive(bool on);
        void setNonBlock(bool on);
        void setIpv6Only(bool on);

        [[nodiscard]] int socketError() const;
        [[nodiscard]] bool isSelfConnect() const;

        [[nodiscard]] static Socket createTcp(DbaseAddressFamily family = AF_INET);
        [[nodiscard]] static Socket createUdp(DbaseAddressFamily family = AF_INET);

    private:
        SocketType m_sock{kInvalidSocket};
};
}  // namespace dbase::net