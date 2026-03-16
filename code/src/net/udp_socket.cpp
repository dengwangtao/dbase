#include "dbase/net/udp_socket.h"

#include "dbase/error/error.h"
#include "dbase/net/socket_ops.h"

#include <stdexcept>
#include <string>

#if defined(_WIN32)
#include <WinSock2.h>
#include <WS2tcpip.h>
#else
#include <cerrno>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace dbase::net
{
namespace
{
[[nodiscard]] bool isWouldBlockErrorCode(int err) noexcept
{
#if defined(_WIN32)
    return err == WSAEWOULDBLOCK;
#else
    return err == EAGAIN || err == EWOULDBLOCK;
#endif
}

[[nodiscard]] int lastSocketErrorCode() noexcept
{
#if defined(_WIN32)
    return ::WSAGetLastError();
#else
    return errno;
#endif
}

[[nodiscard]] dbase::Error makeSocketIoError(const char* what)
{
    const int err = lastSocketErrorCode();
    if (isWouldBlockErrorCode(err))
    {
        return dbase::Error(dbase::ErrorCode::WouldBlock, std::string(what) + ": would block");
    }
    return dbase::makeSystemError(std::string(what), err);
}
}  // namespace

UdpSocket::UdpSocket(Socket socket) noexcept
    : m_socket(std::move(socket))
{
}

dbase::Result<UdpSocket> UdpSocket::create(int family)
{
    SocketType sock = ::socket(family, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == kInvalidSocket)
    {
        return dbase::makeSystemErrorResultT<UdpSocket>("UdpSocket create failed");
    }
    return UdpSocket(Socket(sock));
}

bool UdpSocket::valid() const noexcept
{
    return m_socket.valid();
}

SocketType UdpSocket::fd() const noexcept
{
    return m_socket.fd();
}

void UdpSocket::bindAddress(const InetAddress& addr)
{
    m_socket.bindAddress(addr);
}

void UdpSocket::connect(const InetAddress& addr)
{
    const int ret = ::connect(m_socket.fd(), addr.getSockAddr(), addr.length());
    if (ret != 0)
    {
        throw std::runtime_error("UdpSocket connect failed: " + SocketOps::lastErrorMessage());
    }
}

void UdpSocket::setReuseAddr(bool on)
{
    m_socket.setReuseAddr(on);
}

void UdpSocket::setReusePort(bool on)
{
    m_socket.setReusePort(on);
}

void UdpSocket::setNonBlock(bool on)
{
    m_socket.setNonBlock(on);
}

dbase::Result<std::size_t> UdpSocket::send(std::span<const std::byte> data)
{
    if (!valid())
    {
        return dbase::makeErrorResult<std::size_t>(dbase::ErrorCode::InvalidState, "UdpSocket send on invalid socket");
    }
    if (data.empty())
    {
        return std::size_t{0};
    }

#if defined(_WIN32)
    const int sent = ::send(
            m_socket.fd(),
            reinterpret_cast<const char*>(data.data()),
            static_cast<int>(data.size()),
            0);
#else
    const auto sent = ::send(
            m_socket.fd(),
            reinterpret_cast<const void*>(data.data()),
            data.size(),
            0);
#endif

    if (sent < 0)
    {
        return dbase::Result<std::size_t>(makeSocketIoError("UdpSocket send failed"));
    }

    return static_cast<std::size_t>(sent);
}

dbase::Result<std::size_t> UdpSocket::sendTo(std::span<const std::byte> data, const InetAddress& peer)
{
    if (!valid())
    {
        return dbase::makeErrorResult<std::size_t>(dbase::ErrorCode::InvalidState, "UdpSocket sendTo on invalid socket");
    }
    if (data.empty())
    {
        return std::size_t{0};
    }

#if defined(_WIN32)
    const int sent = ::sendto(
            m_socket.fd(),
            reinterpret_cast<const char*>(data.data()),
            static_cast<int>(data.size()),
            0,
            peer.getSockAddr(),
            peer.length());
#else
    const auto sent = ::sendto(
            m_socket.fd(),
            reinterpret_cast<const void*>(data.data()),
            data.size(),
            0,
            peer.getSockAddr(),
            peer.length());
#endif

    if (sent < 0)
    {
        return dbase::Result<std::size_t>(makeSocketIoError("UdpSocket sendTo failed"));
    }

    return static_cast<std::size_t>(sent);
}

dbase::Result<UdpDatagram> UdpSocket::receiveFrom(std::span<std::byte> buffer)
{
    if (!valid())
    {
        return dbase::makeErrorResult<UdpDatagram>(dbase::ErrorCode::InvalidState, "UdpSocket receiveFrom on invalid socket");
    }
    if (buffer.empty())
    {
        return dbase::makeErrorResult<UdpDatagram>(dbase::ErrorCode::InvalidArgument, "UdpSocket receiveFrom buffer is empty");
    }

    sockaddr_storage storage{};
    socklen_t len = static_cast<socklen_t>(sizeof(storage));

#if defined(_WIN32)
    const int n = ::recvfrom(
            m_socket.fd(),
            reinterpret_cast<char*>(buffer.data()),
            static_cast<int>(buffer.size()),
            0,
            reinterpret_cast<sockaddr*>(&storage),
            &len);
#else
    const auto n = ::recvfrom(
            m_socket.fd(),
            reinterpret_cast<void*>(buffer.data()),
            buffer.size(),
            0,
            reinterpret_cast<sockaddr*>(&storage),
            &len);
#endif

    if (n < 0)
    {
        return dbase::Result<UdpDatagram>(makeSocketIoError("UdpSocket receiveFrom failed"));
    }

    UdpDatagram datagram;
    datagram.size = static_cast<std::size_t>(n);
    datagram.peer = InetAddress(reinterpret_cast<const sockaddr*>(&storage), len);
    return datagram;
}

dbase::Result<std::size_t> UdpSocket::receive(std::span<std::byte> buffer)
{
    if (!valid())
    {
        return dbase::makeErrorResult<std::size_t>(dbase::ErrorCode::InvalidState, "UdpSocket receive on invalid socket");
    }
    if (buffer.empty())
    {
        return dbase::makeErrorResult<std::size_t>(dbase::ErrorCode::InvalidArgument, "UdpSocket receive buffer is empty");
    }

#if defined(_WIN32)
    const int n = ::recv(
            m_socket.fd(),
            reinterpret_cast<char*>(buffer.data()),
            static_cast<int>(buffer.size()),
            0);
#else
    const auto n = ::recv(
            m_socket.fd(),
            reinterpret_cast<void*>(buffer.data()),
            buffer.size(),
            0);
#endif

    if (n < 0)
    {
        return dbase::Result<std::size_t>(makeSocketIoError("UdpSocket receive failed"));
    }

    return static_cast<std::size_t>(n);
}

Socket& UdpSocket::socket() noexcept
{
    return m_socket;
}

const Socket& UdpSocket::socket() const noexcept
{
    return m_socket;
}
}  // namespace dbase::net