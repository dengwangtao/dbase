#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <thread>

#include "dbase/net/inet_address.h"
#include "dbase/net/socket.h"
#include "dbase/net/socket_ops.h"

namespace
{
struct SocketOpsInit
{
        SocketOpsInit()
        {
            static_cast<void>(dbase::net::SocketOps::initialize());
        }
};

const SocketOpsInit kSocketOpsInit{};

using dbase::net::InetAddress;
using dbase::net::kInvalidSocket;
using dbase::net::Socket;
using dbase::net::SocketOps;
using dbase::net::SocketType;

[[nodiscard]] InetAddress ipv4Loopback(std::uint16_t port = 0)
{
    return InetAddress(port, true, false);
}

[[nodiscard]] Socket connectBlocking(const InetAddress& addr)
{
    Socket client = Socket::createTcp(addr.addressFamily());
    client.setNonBlock(false);

    auto ret = SocketOps::connect(client.fd(), addr);
    if (!ret)
    {
        throw std::runtime_error("connect failed: " + ret.error().message());
    }
    return client;
}
}  // namespace

TEST_CASE("Socket default constructed state", "[net][socket]")
{
    Socket socket;

    REQUIRE_FALSE(socket.valid());
    REQUIRE(socket.fd() == kInvalidSocket);
}

TEST_CASE("Socket explicit constructor takes ownership of fd", "[net][socket]")
{
    const SocketType raw = SocketOps::createTcpNonblockingOrDie(AF_INET);

    Socket socket(raw);

    REQUIRE(socket.valid());
    REQUIRE(socket.fd() == raw);

    const SocketType released = socket.release();
    REQUIRE(released == raw);
    REQUIRE_FALSE(socket.valid());

    SocketOps::close(released);
}

TEST_CASE("Socket move constructor transfers ownership", "[net][socket]")
{
    Socket source = Socket::createTcp(AF_INET);
    const SocketType fd = source.fd();

    Socket moved(std::move(source));

    REQUIRE_FALSE(source.valid());
    REQUIRE(source.fd() == kInvalidSocket);
    REQUIRE(moved.valid());
    REQUIRE(moved.fd() == fd);
}

TEST_CASE("Socket move assignment transfers ownership", "[net][socket]")
{
    Socket lhs = Socket::createTcp(AF_INET);
    Socket rhs = Socket::createUdp(AF_INET);

    const SocketType rhsFd = rhs.fd();

    lhs = std::move(rhs);

    REQUIRE(lhs.valid());
    REQUIRE(lhs.fd() == rhsFd);
    REQUIRE_FALSE(rhs.valid());
    REQUIRE(rhs.fd() == kInvalidSocket);
}

TEST_CASE("Socket release detaches owned fd", "[net][socket]")
{
    Socket socket = Socket::createTcp(AF_INET);
    const SocketType fd = socket.fd();

    const SocketType released = socket.release();

    REQUIRE(released == fd);
    REQUIRE_FALSE(socket.valid());
    REQUIRE(socket.fd() == kInvalidSocket);

    SocketOps::close(released);
}

TEST_CASE("Socket reset replaces underlying fd", "[net][socket]")
{
    Socket socket = Socket::createTcp(AF_INET);
    const SocketType newFd = SocketOps::createUdpNonblockingOrDie(AF_INET);

    socket.reset(newFd);

    REQUIRE(socket.valid());
    REQUIRE(socket.fd() == newFd);
}

TEST_CASE("Socket reset without argument clears socket", "[net][socket]")
{
    Socket socket = Socket::createTcp(AF_INET);

    socket.reset();

    REQUIRE_FALSE(socket.valid());
    REQUIRE(socket.fd() == kInvalidSocket);
}

TEST_CASE("Socket createTcp creates valid IPv4 socket", "[net][socket]")
{
    Socket socket = Socket::createTcp(AF_INET);

    REQUIRE(socket.valid());
    REQUIRE(socket.fd() != kInvalidSocket);
}

TEST_CASE("Socket createUdp creates valid IPv4 socket", "[net][socket]")
{
    Socket socket = Socket::createUdp(AF_INET);

    REQUIRE(socket.valid());
    REQUIRE(socket.fd() != kInvalidSocket);
}

TEST_CASE("Socket bindAddress and localAddress work for IPv4 listener", "[net][socket]")
{
    Socket socket = Socket::createTcp(AF_INET);
    socket.setReuseAddr(true);
    socket.bindAddress(ipv4Loopback(0));

    const InetAddress local = socket.localAddress();

    REQUIRE(local.isIpv4());
    REQUIRE(local.isLoopbackIp());
    REQUIRE(local.port() != 0);
}

TEST_CASE("Socket listen succeeds after bind", "[net][socket]")
{
    Socket socket = Socket::createTcp(AF_INET);
    socket.setReuseAddr(true);
    socket.bindAddress(ipv4Loopback(0));

    REQUIRE_NOTHROW(socket.listen());
}

TEST_CASE("Socket accept returns connected socket and peer address", "[net][socket]")
{
    Socket server = Socket::createTcp(AF_INET);
    server.setReuseAddr(true);
    server.bindAddress(ipv4Loopback(0));
    server.listen();

    const InetAddress serverAddr = server.localAddress();

    Socket client = connectBlocking(serverAddr);

    InetAddress peerAddr;
    Socket accepted;

    for (int i = 0; i < 30; ++i)
    {
        try
        {
            accepted = server.accept(&peerAddr);
            break;
        }
        catch (...)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    REQUIRE(accepted.valid());
    REQUIRE(peerAddr.isIpv4());
    REQUIRE(peerAddr.isLoopbackIp());
    REQUIRE(peerAddr.port() != 0);

    const InetAddress acceptedLocal = accepted.localAddress();
    const InetAddress acceptedPeer = accepted.peerAddress();

    REQUIRE(acceptedLocal.toIp() == serverAddr.toIp());
    REQUIRE(acceptedLocal.port() == serverAddr.port());
    REQUIRE(acceptedPeer.toIp() == client.localAddress().toIp());
    REQUIRE(acceptedPeer.port() == client.localAddress().port());
}

TEST_CASE("Socket localAddress and peerAddress reflect connected endpoints", "[net][socket]")
{
    Socket server = Socket::createTcp(AF_INET);
    server.setReuseAddr(true);
    server.bindAddress(ipv4Loopback(0));
    server.listen();

    const InetAddress serverAddr = server.localAddress();
    Socket client = connectBlocking(serverAddr);

    InetAddress peerAddr;
    Socket accepted;

    for (int i = 0; i < 30; ++i)
    {
        try
        {
            accepted = server.accept(&peerAddr);
            break;
        }
        catch (...)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    REQUIRE(client.localAddress().isIpv4());
    REQUIRE(client.peerAddress().isIpv4());
    REQUIRE(client.peerAddress().toIp() == serverAddr.toIp());
    REQUIRE(client.peerAddress().port() == serverAddr.port());

    REQUIRE(accepted.localAddress().toIp() == serverAddr.toIp());
    REQUIRE(accepted.localAddress().port() == serverAddr.port());
    REQUIRE(accepted.peerAddress().toIp() == client.localAddress().toIp());
    REQUIRE(accepted.peerAddress().port() == client.localAddress().port());
}

TEST_CASE("Socket socketError is readable on valid socket", "[net][socket]")
{
    Socket socket = Socket::createTcp(AF_INET);

    REQUIRE(socket.socketError() == 0);
}

TEST_CASE("Socket setReuseAddr and setReusePort do not throw on valid socket", "[net][socket]")
{
    Socket socket = Socket::createTcp(AF_INET);

    REQUIRE_NOTHROW(socket.setReuseAddr(true));
    REQUIRE_NOTHROW(socket.setReuseAddr(false));

#if defined(_WIN32)
    REQUIRE_THROWS_WITH(
            socket.setReusePort(true),
            Catch::Matchers::ContainsSubstring("SO_REUSEPORT"));

    REQUIRE_THROWS_WITH(
            socket.setReusePort(false),
            Catch::Matchers::ContainsSubstring("SO_REUSEPORT"));
#else
    REQUIRE_NOTHROW(socket.setReusePort(true));
    REQUIRE_NOTHROW(socket.setReusePort(false));
#endif
}

TEST_CASE("Socket setTcpNoDelay and setKeepAlive do not throw on valid TCP socket", "[net][socket]")
{
    Socket socket = Socket::createTcp(AF_INET);

    REQUIRE_NOTHROW(socket.setTcpNoDelay(true));
    REQUIRE_NOTHROW(socket.setTcpNoDelay(false));

    REQUIRE_NOTHROW(socket.setKeepAlive(true));
    REQUIRE_NOTHROW(socket.setKeepAlive(false));
}

TEST_CASE("Socket setNonBlock toggles blocking mode", "[net][socket]")
{
    Socket socket = Socket::createTcp(AF_INET);

    REQUIRE_NOTHROW(socket.setNonBlock(false));
    REQUIRE_NOTHROW(socket.setNonBlock(true));
}

TEST_CASE("Socket setIpv6Only works on IPv6 socket", "[net][socket]")
{
    Socket socket = Socket::createTcp(AF_INET6);

    REQUIRE_NOTHROW(socket.setIpv6Only(true));
    REQUIRE_NOTHROW(socket.setIpv6Only(false));
}

TEST_CASE("Socket shutdownWrite does not throw on connected socket", "[net][socket]")
{
    Socket server = Socket::createTcp(AF_INET);
    server.setReuseAddr(true);
    server.bindAddress(ipv4Loopback(0));
    server.listen();

    const InetAddress serverAddr = server.localAddress();
    Socket client = connectBlocking(serverAddr);

    InetAddress peerAddr;
    Socket accepted;

    for (int i = 0; i < 30; ++i)
    {
        try
        {
            accepted = server.accept(&peerAddr);
            break;
        }
        catch (...)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    REQUIRE(accepted.valid());
    REQUIRE_NOTHROW(client.shutdownWrite());
}

TEST_CASE("Socket isSelfConnect returns false for normal accepted connection", "[net][socket]")
{
    Socket server = Socket::createTcp(AF_INET);
    server.setReuseAddr(true);
    server.bindAddress(ipv4Loopback(0));
    server.listen();

    const InetAddress serverAddr = server.localAddress();
    Socket client = connectBlocking(serverAddr);

    InetAddress peerAddr;
    Socket accepted;

    for (int i = 0; i < 30; ++i)
    {
        try
        {
            accepted = server.accept(&peerAddr);
            break;
        }
        catch (...)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    REQUIRE_FALSE(client.isSelfConnect());
    REQUIRE_FALSE(accepted.isSelfConnect());
}

TEST_CASE("Socket bindAddress can bind IPv6 loopback socket", "[net][socket]")
{
    Socket socket = Socket::createTcp(AF_INET6);
    socket.setReuseAddr(true);
    socket.setIpv6Only(true);
    socket.bindAddress(InetAddress(0, true, true));

    const InetAddress local = socket.localAddress();

    REQUIRE(local.isIpv6());
    REQUIRE(local.isLoopbackIp());
    REQUIRE(local.port() != 0);
}

TEST_CASE("Socket accept without pending connection eventually throws on nonblocking listener", "[net][socket]")
{
    Socket server = Socket::createTcp(AF_INET);
    server.setReuseAddr(true);
    server.bindAddress(ipv4Loopback(0));
    server.listen();

    InetAddress peerAddr;
    REQUIRE_THROWS(server.accept(&peerAddr));
}