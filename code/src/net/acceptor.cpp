#include "dbase/net/acceptor.h"

#include "dbase/net/socket_ops.h"

#include <stdexcept>
#include <utility>

#if defined(_WIN32)
#include <WinSock2.h>
#else
#include <cerrno>
#endif

namespace dbase::net
{
namespace
{
bool isWouldBlockError(int err) noexcept
{
#if defined(_WIN32)
    return err == WSAEWOULDBLOCK;
#else
    return err == EAGAIN || err == EWOULDBLOCK;
#endif
}

bool isInterruptedError(int err) noexcept
{
#if defined(_WIN32)
    return err == WSAEINTR;
#else
    return err == EINTR;
#endif
}

bool isTransientAcceptError(int err) noexcept
{
#if defined(_WIN32)
    return err == WSAECONNRESET || err == WSAECONNABORTED;
#else
    return err == ECONNABORTED || err == EPROTO || err == EPERM;
#endif
}
}  // namespace

Acceptor::Acceptor(const InetAddress& listenAddr, bool reusePort, bool ipv6Only)
    : m_socket(SocketOps::createTcpNonblockingOrDie(listenAddr.addressFamily())),
      m_listenAddr(listenAddr)
{
    auto reuseAddrRet = SocketOps::setReuseAddr(m_socket.fd(), true);
    if (!reuseAddrRet)
    {
        throw std::runtime_error("Acceptor setReuseAddr failed: " + reuseAddrRet.error().message());
    }

    if (listenAddr.addressFamily() == AF_INET6)
    {
        auto ipv6OnlyRet = SocketOps::setIpv6Only(m_socket.fd(), ipv6Only);
        if (!ipv6OnlyRet)
        {
            throw std::runtime_error("Acceptor setIpv6Only failed: " + ipv6OnlyRet.error().message());
        }
    }

    if (reusePort)
    {
        auto reusePortRet = SocketOps::setReusePort(m_socket.fd(), true);
        if (!reusePortRet)
        {
            throw std::runtime_error("Acceptor setReusePort failed: " + reusePortRet.error().message());
        }
    }

    auto bindRet = SocketOps::bind(m_socket.fd(), m_listenAddr);
    if (!bindRet)
    {
        throw std::runtime_error("Acceptor bind failed: " + bindRet.error().message());
    }
}

void Acceptor::setNewConnectionCallback(NewConnectionCallback cb)
{
    m_newConnectionCallback = std::move(cb);
}

void Acceptor::listen()
{
    if (m_listening)
    {
        return;
    }

    auto listenRet = SocketOps::listen(m_socket.fd());
    if (!listenRet)
    {
        throw std::runtime_error("Acceptor listen failed: " + listenRet.error().message());
    }

    m_listening = true;
}

bool Acceptor::listening() const noexcept
{
    return m_listening;
}

const InetAddress& Acceptor::listenAddress() const noexcept
{
    return m_listenAddr;
}

Socket& Acceptor::socket() noexcept
{
    return m_socket;
}

const Socket& Acceptor::socket() const noexcept
{
    return m_socket;
}

void Acceptor::setEdgeTriggered(bool on) noexcept
{
#if defined(__linux__)
    m_edgeTriggered = on;
#else
    m_edgeTriggered = false;
    (void)on;
#endif
}

bool Acceptor::edgeTriggered() const noexcept
{
    return m_edgeTriggered;
}

std::size_t Acceptor::acceptAvailable()
{
    if (!m_listening)
    {
        throw std::logic_error("Acceptor is not listening");
    }

    std::size_t acceptedCount = 0;

    for (;;)
    {
        InetAddress peerAddr;
        auto acceptRet = SocketOps::accept(m_socket.fd(), &peerAddr);
        if (acceptRet)
        {
            ++acceptedCount;

            if (m_newConnectionCallback)
            {
                m_newConnectionCallback(Socket(acceptRet.value()), peerAddr);
            }
            else
            {
                Socket orphan(acceptRet.value());
            }

            continue;
        }

        const int err =
#if defined(_WIN32)
                ::WSAGetLastError();
#else
                errno;
#endif

        if (isWouldBlockError(err))
        {
            break;
        }

        if (isInterruptedError(err) || isTransientAcceptError(err))
        {
            continue;
        }

        throw std::runtime_error("Acceptor accept failed: " + SocketOps::lastErrorMessage());
    }

    return acceptedCount;
}

}  // namespace dbase::net