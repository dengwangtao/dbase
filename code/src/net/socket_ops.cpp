#include "dbase/net/socket_ops.h"

#include <cstring>
#include <stdexcept>
#include <utility>

#if defined(_WIN32)
#include <MSWSock.h>
#include <WS2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace dbase::net
{
namespace
{
[[nodiscard]] dbase::Error makeSystemError(std::string message)
{
    return dbase::Error(dbase::ErrorCode::SystemError, std::move(message));
}

[[nodiscard]] int lastErrorCode() noexcept
{
#if defined(_WIN32)
    return ::WSAGetLastError();
#else
    return errno;
#endif
}

[[nodiscard]] std::string errorMessageFromCode(int code)
{
#if defined(_WIN32)
    char* buffer = nullptr;
    const DWORD size = ::FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            static_cast<DWORD>(code),
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPSTR>(&buffer),
            0,
            nullptr);

    std::string message;
    if (size != 0 && buffer != nullptr)
    {
        message.assign(buffer, size);
        ::LocalFree(buffer);
    }
    else
    {
        message = "unknown windows socket error";
    }

    return message;
#else
    return std::strerror(code);
#endif
}

[[nodiscard]] sockaddr* sockaddrCast(sockaddr_in* addr) noexcept
{
    return reinterpret_cast<sockaddr*>(addr);
}

[[nodiscard]] const sockaddr* sockaddrCast(const sockaddr_in* addr) noexcept
{
    return reinterpret_cast<const sockaddr*>(addr);
}
}  // namespace

dbase::Result<void> SocketOps::initialize()
{
#if defined(_WIN32)
    WSADATA wsaData;
    const int rc = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (rc != 0)
    {
        return dbase::Result<void>(makeSystemError("WSAStartup failed: " + errorMessageFromCode(rc)));
    }
#endif
    return dbase::Result<void>();
}

void SocketOps::cleanup() noexcept
{
#if defined(_WIN32)
    ::WSACleanup();
#endif
}

SocketType SocketOps::createTcpNonblockingOrDie()
{
#if defined(_WIN32)
    SocketType sock = ::WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (sock == kInvalidSocket)
    {
        throw std::runtime_error("createTcpNonblockingOrDie failed: " + lastErrorMessage());
    }

    if (auto ret = setNonBlock(sock, true); !ret)
    {
        close(sock);
        throw std::runtime_error("createTcpNonblockingOrDie setNonBlock failed: " + ret.error().message());
    }

    return sock;
#else
    SocketType sock = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
    if (sock == kInvalidSocket)
    {
        throw std::runtime_error("createTcpNonblockingOrDie failed: " + lastErrorMessage());
    }

    return sock;
#endif
}

SocketType SocketOps::createUdpNonblockingOrDie()
{
#if defined(_WIN32)
    SocketType sock = ::WSASocketW(AF_INET, SOCK_DGRAM, IPPROTO_UDP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (sock == kInvalidSocket)
    {
        throw std::runtime_error("createUdpNonblockingOrDie failed: " + lastErrorMessage());
    }

    if (auto ret = setNonBlock(sock, true); !ret)
    {
        close(sock);
        throw std::runtime_error("createUdpNonblockingOrDie setNonBlock failed: " + ret.error().message());
    }

    return sock;
#else
    SocketType sock = ::socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_UDP);
    if (sock == kInvalidSocket)
    {
        throw std::runtime_error("createUdpNonblockingOrDie failed: " + lastErrorMessage());
    }

    return sock;
#endif
}

void SocketOps::close(SocketType sock) noexcept
{
    if (sock == kInvalidSocket)
    {
        return;
    }

#if defined(_WIN32)
    ::closesocket(sock);
#else
    ::close(sock);
#endif
}

void SocketOps::shutdownWrite(SocketType sock)
{
#if defined(_WIN32)
    ::shutdown(sock, SD_SEND);
#else
    ::shutdown(sock, SHUT_WR);
#endif
}

dbase::Result<void> SocketOps::bind(SocketType sock, const InetAddress& addr)
{
    const int rc = ::bind(sock, sockaddrCast(&addr.getSockAddrInet()), static_cast<int>(sizeof(sockaddr_in)));
    if (rc != 0)
    {
        return dbase::Result<void>(makeSystemError("bind failed: " + lastErrorMessage()));
    }

    return dbase::Result<void>();
}

dbase::Result<void> SocketOps::listen(SocketType sock, int backlog)
{
    const int rc = ::listen(sock, backlog);
    if (rc != 0)
    {
        return dbase::Result<void>(makeSystemError("listen failed: " + lastErrorMessage()));
    }

    return dbase::Result<void>();
}

dbase::Result<SocketType> SocketOps::accept(SocketType sock, InetAddress* peerAddr)
{
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    int len = static_cast<int>(sizeof(addr));

#if defined(_WIN32)
    SocketType conn = ::accept(sock, sockaddrCast(&addr), &len);
#else
    socklen_t sockLen = static_cast<socklen_t>(sizeof(addr));
    SocketType conn = ::accept4(sock, sockaddrCast(&addr), &sockLen, SOCK_NONBLOCK | SOCK_CLOEXEC);
#endif

    if (conn == kInvalidSocket)
    {
        return dbase::Result<SocketType>(makeSystemError("accept failed: " + lastErrorMessage()));
    }

#if defined(_WIN32)
    if (auto ret = setNonBlock(conn, true); !ret)
    {
        close(conn);
        return dbase::Result<SocketType>(ret.error());
    }
#endif

    if (peerAddr != nullptr)
    {
        peerAddr->setSockAddrInet(addr);
    }

    return dbase::Result<SocketType>(conn);
}

dbase::Result<void> SocketOps::connect(SocketType sock, const InetAddress& addr)
{
    const int rc = ::connect(sock, sockaddrCast(&addr.getSockAddrInet()), static_cast<int>(sizeof(sockaddr_in)));
    if (rc != 0)
    {
        const int err = lastErrorCode();
#if defined(_WIN32)
        if (err == WSAEWOULDBLOCK || err == WSAEINPROGRESS || err == WSAEALREADY)
#else
        if (err == EINPROGRESS || err == EALREADY)
#endif
        {
            return dbase::Result<void>();
        }

        return dbase::Result<void>(makeSystemError("connect failed: " + errorMessageFromCode(err)));
    }

    return dbase::Result<void>();
}

int SocketOps::read(SocketType sock, void* buf, std::size_t len)
{
#if defined(_WIN32)
    return ::recv(sock, static_cast<char*>(buf), static_cast<int>(len), 0);
#else
    return static_cast<int>(::read(sock, buf, len));
#endif
}

int SocketOps::write(SocketType sock, const void* buf, std::size_t len)
{
#if defined(_WIN32)
    return ::send(sock, static_cast<const char*>(buf), static_cast<int>(len), 0);
#else
    return static_cast<int>(::write(sock, buf, len));
#endif
}

dbase::Result<InetAddress> SocketOps::localAddress(SocketType sock)
{
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));

#if defined(_WIN32)
    int len = static_cast<int>(sizeof(addr));
#else
    socklen_t len = static_cast<socklen_t>(sizeof(addr));
#endif

    if (::getsockname(sock, sockaddrCast(&addr), &len) != 0)
    {
        return dbase::Result<InetAddress>(makeSystemError("getsockname failed: " + lastErrorMessage()));
    }

    return dbase::Result<InetAddress>(InetAddress(addr));
}

dbase::Result<InetAddress> SocketOps::peerAddress(SocketType sock)
{
    sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));

#if defined(_WIN32)
    int len = static_cast<int>(sizeof(addr));
#else
    socklen_t len = static_cast<socklen_t>(sizeof(addr));
#endif

    if (::getpeername(sock, sockaddrCast(&addr), &len) != 0)
    {
        return dbase::Result<InetAddress>(makeSystemError("getpeername failed: " + lastErrorMessage()));
    }

    return dbase::Result<InetAddress>(InetAddress(addr));
}

dbase::Result<void> SocketOps::setReuseAddr(SocketType sock, bool on)
{
    const int opt = on ? 1 : 0;
    if (::setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt)) != 0)
    {
        return dbase::Result<void>(makeSystemError("setsockopt SO_REUSEADDR failed: " + lastErrorMessage()));
    }

    return dbase::Result<void>();
}

dbase::Result<void> SocketOps::setReusePort(SocketType sock, bool on)
{
#if defined(_WIN32)
    (void)sock;
    (void)on;
    return dbase::Result<void>(makeSystemError("SO_REUSEPORT is not supported on Windows"));
#else
    const int opt = on ? 1 : 0;
    if (::setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) != 0)
    {
        return dbase::Result<void>(makeSystemError("setsockopt SO_REUSEPORT failed: " + lastErrorMessage()));
    }

    return dbase::Result<void>();
#endif
}

dbase::Result<void> SocketOps::setTcpNoDelay(SocketType sock, bool on)
{
    const int opt = on ? 1 : 0;
    if (::setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&opt), sizeof(opt)) != 0)
    {
        return dbase::Result<void>(makeSystemError("setsockopt TCP_NODELAY failed: " + lastErrorMessage()));
    }

    return dbase::Result<void>();
}

dbase::Result<void> SocketOps::setKeepAlive(SocketType sock, bool on)
{
    const int opt = on ? 1 : 0;
    if (::setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<const char*>(&opt), sizeof(opt)) != 0)
    {
        return dbase::Result<void>(makeSystemError("setsockopt SO_KEEPALIVE failed: " + lastErrorMessage()));
    }

    return dbase::Result<void>();
}

dbase::Result<void> SocketOps::setNonBlock(SocketType sock, bool on)
{
#if defined(_WIN32)
    u_long mode = on ? 1UL : 0UL;
    if (::ioctlsocket(sock, FIONBIO, &mode) != 0)
    {
        return dbase::Result<void>(makeSystemError("ioctlsocket FIONBIO failed: " + lastErrorMessage()));
    }
    return dbase::Result<void>();
#else
    const int flags = ::fcntl(sock, F_GETFL, 0);
    if (flags < 0)
    {
        return dbase::Result<void>(makeSystemError("fcntl F_GETFL failed: " + lastErrorMessage()));
    }

    const int newFlags = on ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    if (::fcntl(sock, F_SETFL, newFlags) != 0)
    {
        return dbase::Result<void>(makeSystemError("fcntl F_SETFL failed: " + lastErrorMessage()));
    }

    return dbase::Result<void>();
#endif
}

int SocketOps::getSocketError(SocketType sock)
{
    int opt = 0;
#if defined(_WIN32)
    int len = static_cast<int>(sizeof(opt));
#else
    socklen_t len = static_cast<socklen_t>(sizeof(opt));
#endif

    if (::getsockopt(sock, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&opt), &len) < 0)
    {
        return lastErrorCode();
    }

    return opt;
}

std::string SocketOps::lastErrorMessage()
{
    return errorMessageFromCode(lastErrorCode());
}

}  // namespace dbase::net