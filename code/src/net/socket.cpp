#include "dbase/net/socket.h"

#include <stdexcept>
#include <utility>

namespace dbase::net
{
namespace
{
void throwIfError(const dbase::Result<void>& result, const char* what)
{
    if (!result)
    {
        throw std::runtime_error(std::string(what) + ": " + result.error().message());
    }
}

template <typename T>
T unwrapOrThrow(dbase::Result<T>&& result, const char* what)
{
    if (!result)
    {
        throw std::runtime_error(std::string(what) + ": " + result.error().message());
    }

    return std::move(result.value());
}
}  // namespace

Socket::Socket(SocketType sock) noexcept
    : m_sock(sock)
{
}

Socket::~Socket()
{
    reset();
}

Socket::Socket(Socket&& other) noexcept
    : m_sock(other.release())
{
}

Socket& Socket::operator=(Socket&& other) noexcept
{
    if (this != &other)
    {
        reset(other.release());
    }
    return *this;
}

bool Socket::valid() const noexcept
{
    return m_sock != kInvalidSocket;
}

SocketType Socket::fd() const noexcept
{
    return m_sock;
}

SocketType Socket::release() noexcept
{
    const SocketType sock = m_sock;
    m_sock = kInvalidSocket;
    return sock;
}

void Socket::reset(SocketType sock) noexcept
{
    if (m_sock != kInvalidSocket)
    {
        SocketOps::close(m_sock);
    }
    m_sock = sock;
}

void Socket::bindAddress(const InetAddress& addr)
{
    throwIfError(SocketOps::bind(m_sock, addr), "Socket::bindAddress failed");
}

void Socket::listen(int backlog)
{
    throwIfError(SocketOps::listen(m_sock, backlog), "Socket::listen failed");
}

Socket Socket::accept(InetAddress* peerAddr)
{
    auto result = SocketOps::accept(m_sock, peerAddr);
    return Socket(unwrapOrThrow(std::move(result), "Socket::accept failed"));
}

void Socket::shutdownWrite()
{
    SocketOps::shutdownWrite(m_sock);
}

InetAddress Socket::localAddress() const
{
    return unwrapOrThrow(SocketOps::localAddress(m_sock), "Socket::localAddress failed");
}

InetAddress Socket::peerAddress() const
{
    return unwrapOrThrow(SocketOps::peerAddress(m_sock), "Socket::peerAddress failed");
}

void Socket::setReuseAddr(bool on)
{
    throwIfError(SocketOps::setReuseAddr(m_sock, on), "Socket::setReuseAddr failed");
}

void Socket::setReusePort(bool on)
{
    throwIfError(SocketOps::setReusePort(m_sock, on), "Socket::setReusePort failed");
}

void Socket::setTcpNoDelay(bool on)
{
    throwIfError(SocketOps::setTcpNoDelay(m_sock, on), "Socket::setTcpNoDelay failed");
}

void Socket::setKeepAlive(bool on)
{
    throwIfError(SocketOps::setKeepAlive(m_sock, on), "Socket::setKeepAlive failed");
}

void Socket::setNonBlock(bool on)
{
    throwIfError(SocketOps::setNonBlock(m_sock, on), "Socket::setNonBlock failed");
}

void Socket::setIpv6Only(bool on)
{
    throwIfError(SocketOps::setIpv6Only(m_sock, on), "Socket::setIpv6Only failed");
}

int Socket::socketError() const
{
    return SocketOps::getSocketError(m_sock);
}

bool Socket::isSelfConnect() const
{
    const auto local = localAddress();
    const auto peer = peerAddress();

    if (local.addressFamily() != peer.addressFamily())
    {
        return false;
    }

    if (local.port() != peer.port())
    {
        return false;
    }

    return local.toIp() == peer.toIp();
}

Socket Socket::createTcp(DbaseAddressFamily family)
{
    return Socket(SocketOps::createTcpNonblockingOrDie(family));
}

Socket Socket::createUdp(DbaseAddressFamily family)
{
    return Socket(SocketOps::createUdpNonblockingOrDie(family));
}

}  // namespace dbase::net