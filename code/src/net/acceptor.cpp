#include "dbase/net/acceptor.h"

#include <stdexcept>
#include <utility>

#if defined(_WIN32)
#include <WinSock2.h>
#else
#include <cerrno>
#include <sys/socket.h>
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
}  // namespace

Acceptor::Acceptor(const InetAddress& listenAddr, bool reusePort, bool ipv6Only)
    : m_listenAddr(listenAddr),
      m_acceptSocket(Socket::createTcp(listenAddr.addressFamily()))
{
    m_acceptSocket.setReuseAddr(true);

    if (reusePort)
    {
        try
        {
            m_acceptSocket.setReusePort(true);
        }
        catch (...)
        {
        }
    }

    if (listenAddr.isIpv6())
    {
        try
        {
            m_acceptSocket.setIpv6Only(ipv6Only);
        }
        catch (...)
        {
        }
    }

    m_acceptSocket.bindAddress(m_listenAddr);
}

void Acceptor::setNewConnectionCallback(NewConnectionCallback cb)
{
    m_newConnectionCallback = std::move(cb);
}

void Acceptor::listen(int backlog)
{
    if (m_listening)
    {
        throw std::logic_error("Acceptor already listening");
    }

    m_acceptSocket.listen(backlog);
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

const Socket& Acceptor::socket() const noexcept
{
    return m_acceptSocket;
}

std::optional<std::pair<Socket, InetAddress>> Acceptor::acceptOnce()
{
    if (!m_listening)
    {
        throw std::logic_error("Acceptor is not listening");
    }

#if defined(_WIN32)
    sockaddr_storage addr;
    int len = static_cast<int>(sizeof(addr));
    SocketType conn = ::accept(m_acceptSocket.fd(), reinterpret_cast<sockaddr*>(&addr), &len);
    if (conn == kInvalidSocket)
    {
        const int err = ::WSAGetLastError();
        if (isWouldBlockError(err))
        {
            return std::nullopt;
        }

        throw std::runtime_error(
                "Acceptor::acceptOnce failed: " + SocketOps::lastErrorMessage());
    }

    auto nonblockRet = SocketOps::setNonBlock(conn, true);
    if (!nonblockRet)
    {
        SocketOps::close(conn);
        throw std::runtime_error(
                "Acceptor::acceptOnce setNonBlock failed: " + nonblockRet.error().message());
    }

    InetAddress peerAddr(reinterpret_cast<const sockaddr*>(&addr), static_cast<socklen_t>(len));
#else
    sockaddr_storage addr;
    socklen_t len = static_cast<socklen_t>(sizeof(addr));
    SocketType conn = ::accept4(
            m_acceptSocket.fd(),
            reinterpret_cast<sockaddr*>(&addr),
            &len,
            SOCK_NONBLOCK | SOCK_CLOEXEC);

    if (conn == kInvalidSocket)
    {
        const int err = errno;
        if (isWouldBlockError(err))
        {
            return std::nullopt;
        }

        throw std::runtime_error(
                "Acceptor::acceptOnce failed: " + SocketOps::lastErrorMessage());
    }

    InetAddress peerAddr(reinterpret_cast<const sockaddr*>(&addr), len);
#endif

    Socket socket(conn);

    if (m_newConnectionCallback)
    {
        m_newConnectionCallback(Socket(socket.release()), peerAddr);
        return std::nullopt;
    }

    return std::make_optional(std::make_pair(std::move(socket), peerAddr));
}

std::size_t Acceptor::acceptAvailable(std::size_t maxAcceptCount)
{
    std::size_t accepted = 0;

    while (accepted < maxAcceptCount)
    {
        auto conn = acceptOnce();
        if (!conn.has_value())
        {
            break;
        }

        ++accepted;

        if (m_newConnectionCallback)
        {
            continue;
        }
    }

    return accepted;
}

}  // namespace dbase::net