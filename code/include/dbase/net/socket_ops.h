#pragma once

#include "dbase/error/error.h"
#include "dbase/net/inet_address.h"

#include <cstdint>
#include <string>

#if defined(_WIN32)
#include <WinSock2.h>
#else
#include <sys/types.h>
#endif

namespace dbase::net
{
#if defined(_WIN32)
using SocketType = SOCKET;
constexpr SocketType kInvalidSocket = INVALID_SOCKET;
#else
using SocketType = int;
constexpr SocketType kInvalidSocket = -1;
#endif

class SocketOps
{
    public:
        static dbase::Result<void> initialize();
        static void cleanup() noexcept;

        [[nodiscard]] static SocketType createTcpNonblockingOrDie(DbaseAddressFamily family = AF_INET);
        [[nodiscard]] static SocketType createUdpNonblockingOrDie(DbaseAddressFamily family = AF_INET);

        static void close(SocketType sock) noexcept;
        static void shutdownWrite(SocketType sock);

        [[nodiscard]] static dbase::Result<void> bind(SocketType sock, const InetAddress& addr);
        [[nodiscard]] static dbase::Result<void> listen(SocketType sock, int backlog = SOMAXCONN);
        [[nodiscard]] static dbase::Result<SocketType> accept(SocketType sock, InetAddress* peerAddr);

        [[nodiscard]] static dbase::Result<void> connect(SocketType sock, const InetAddress& addr);

        [[nodiscard]] static int read(SocketType sock, void* buf, std::size_t len);
        [[nodiscard]] static int write(SocketType sock, const void* buf, std::size_t len);

        [[nodiscard]] static dbase::Result<InetAddress> localAddress(SocketType sock);
        [[nodiscard]] static dbase::Result<InetAddress> peerAddress(SocketType sock);

        static dbase::Result<void> setReuseAddr(SocketType sock, bool on);
        static dbase::Result<void> setReusePort(SocketType sock, bool on);
        static dbase::Result<void> setTcpNoDelay(SocketType sock, bool on);
        static dbase::Result<void> setKeepAlive(SocketType sock, bool on);
        static dbase::Result<void> setNonBlock(SocketType sock, bool on);
        static dbase::Result<void> setIpv6Only(SocketType sock, bool on);

        [[nodiscard]] static int getSocketError(SocketType sock);
        [[nodiscard]] static std::string lastErrorMessage();
};
}  // namespace dbase::net