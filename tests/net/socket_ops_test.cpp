#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <thread>
#include <fstream>

#include "dbase/net/inet_address.h"
#include "dbase/net/socket_ops.h"

namespace
{
using dbase::net::InetAddress;
using dbase::net::kInvalidSocket;
using dbase::net::SocketOps;
using dbase::net::SocketType;

struct SocketOpsGlobalInit
{
        SocketOpsGlobalInit()
        {
            auto ret = SocketOps::initialize();
            if (!ret)
            {
                throw std::runtime_error(ret.error().message());
            }
        }

        ~SocketOpsGlobalInit()
        {
            SocketOps::cleanup();
        }
};

const SocketOpsGlobalInit kSocketOpsGlobalInit{};

[[nodiscard]] InetAddress ipv4Loopback(std::uint16_t port = 0)
{
    return InetAddress(port, true, false);
}

void closeIfValid(SocketType fd)
{
    if (fd != kInvalidSocket)
    {
        SocketOps::close(fd);
    }
}

[[nodiscard]] SocketType connectBlocking(const InetAddress& addr)
{
    const SocketType sock = SocketOps::createTcpNonblockingOrDie(addr.addressFamily());

    auto nonblockRet = SocketOps::setNonBlock(sock, false);
    if (!nonblockRet)
    {
        SocketOps::close(sock);
        throw std::runtime_error(nonblockRet.error().message());
    }

    auto connectRet = SocketOps::connect(sock, addr);
    if (!connectRet)
    {
        SocketOps::close(sock);
        throw std::runtime_error(connectRet.error().message());
    }

    return sock;
}

bool isLinuxIpv6Disabled()
{
#if defined(__linux__)
    auto readFlag = [](const char* path) -> bool
    {
        std::ifstream ifs(path);
        std::string value;
        if (!(ifs >> value))
        {
            return false;
        }
        return value == "1";
    };

    return readFlag("/proc/sys/net/ipv6/conf/all/disable_ipv6") || readFlag("/proc/sys/net/ipv6/conf/default/disable_ipv6") || readFlag("/proc/sys/net/ipv6/conf/lo/disable_ipv6");
#else
    return false;
#endif
}

}  // namespace

TEST_CASE("SocketOps initialize succeeds", "[net][socket_ops]")
{
    auto ret = SocketOps::initialize();
    REQUIRE(ret);
    SocketOps::cleanup();
}

TEST_CASE("SocketOps createTcpNonblockingOrDie creates valid socket", "[net][socket_ops]")
{
    const SocketType sock = SocketOps::createTcpNonblockingOrDie(AF_INET);

    REQUIRE(sock != kInvalidSocket);

    closeIfValid(sock);
}

TEST_CASE("SocketOps createUdpNonblockingOrDie creates valid socket", "[net][socket_ops]")
{
    const SocketType sock = SocketOps::createUdpNonblockingOrDie(AF_INET);

    REQUIRE(sock != kInvalidSocket);

    closeIfValid(sock);
}

TEST_CASE("SocketOps close accepts invalid socket harmlessly", "[net][socket_ops]")
{
    REQUIRE_NOTHROW(SocketOps::close(kInvalidSocket));
}

TEST_CASE("SocketOps bind succeeds for loopback IPv4 address", "[net][socket_ops]")
{
    const SocketType sock = SocketOps::createTcpNonblockingOrDie(AF_INET);

    auto reuseRet = SocketOps::setReuseAddr(sock, true);
    REQUIRE(reuseRet);

    auto bindRet = SocketOps::bind(sock, ipv4Loopback(0));
    REQUIRE(bindRet);

    auto localRet = SocketOps::localAddress(sock);
    REQUIRE(localRet);
    REQUIRE(localRet.value().isIpv4());
    REQUIRE(localRet.value().isLoopbackIp());
    REQUIRE(localRet.value().port() != 0);

    closeIfValid(sock);
}

TEST_CASE("SocketOps listen succeeds after bind", "[net][socket_ops]")
{
    const SocketType sock = SocketOps::createTcpNonblockingOrDie(AF_INET);

    REQUIRE(SocketOps::setReuseAddr(sock, true));
    REQUIRE(SocketOps::bind(sock, ipv4Loopback(0)));

    auto listenRet = SocketOps::listen(sock);
    REQUIRE(listenRet);

    closeIfValid(sock);
}

TEST_CASE("SocketOps accept returns error when no pending connection exists", "[net][socket_ops]")
{
    const SocketType server = SocketOps::createTcpNonblockingOrDie(AF_INET);

    REQUIRE(SocketOps::setReuseAddr(server, true));
    REQUIRE(SocketOps::bind(server, ipv4Loopback(0)));
    REQUIRE(SocketOps::listen(server));

    InetAddress peerAddr;
    auto acceptRet = SocketOps::accept(server, &peerAddr);

    REQUIRE_FALSE(acceptRet);
    REQUIRE_THAT(acceptRet.error().message(), Catch::Matchers::ContainsSubstring("accept failed"));

    closeIfValid(server);
}

TEST_CASE("SocketOps connect and accept succeed for loopback TCP connection", "[net][socket_ops]")
{
    const SocketType server = SocketOps::createTcpNonblockingOrDie(AF_INET);
    REQUIRE(SocketOps::setReuseAddr(server, true));
    REQUIRE(SocketOps::bind(server, ipv4Loopback(0)));
    REQUIRE(SocketOps::listen(server));

    auto serverAddrRet = SocketOps::localAddress(server);
    REQUIRE(serverAddrRet);

    const InetAddress serverAddr = serverAddrRet.value();
    const SocketType client = connectBlocking(serverAddr);

    InetAddress peerAddr;
    SocketType accepted = kInvalidSocket;

    for (int i = 0; i < 30 && accepted == kInvalidSocket; ++i)
    {
        auto acceptRet = SocketOps::accept(server, &peerAddr);
        if (acceptRet)
        {
            accepted = acceptRet.value();
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    REQUIRE(accepted != kInvalidSocket);
    REQUIRE(peerAddr.isIpv4());
    REQUIRE(peerAddr.isLoopbackIp());
    REQUIRE(peerAddr.port() != 0);

    closeIfValid(accepted);
    closeIfValid(client);
    closeIfValid(server);
}

TEST_CASE("SocketOps localAddress and peerAddress reflect connected endpoints", "[net][socket_ops]")
{
    const SocketType server = SocketOps::createTcpNonblockingOrDie(AF_INET);
    REQUIRE(SocketOps::setReuseAddr(server, true));
    REQUIRE(SocketOps::bind(server, ipv4Loopback(0)));
    REQUIRE(SocketOps::listen(server));

    const InetAddress serverAddr = SocketOps::localAddress(server).value();
    const SocketType client = connectBlocking(serverAddr);

    InetAddress peerAddr;
    SocketType accepted = kInvalidSocket;

    for (int i = 0; i < 30 && accepted == kInvalidSocket; ++i)
    {
        auto acceptRet = SocketOps::accept(server, &peerAddr);
        if (acceptRet)
        {
            accepted = acceptRet.value();
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    REQUIRE(accepted != kInvalidSocket);

    const auto clientLocal = SocketOps::localAddress(client);
    const auto clientPeer = SocketOps::peerAddress(client);
    const auto acceptedLocal = SocketOps::localAddress(accepted);
    const auto acceptedPeer = SocketOps::peerAddress(accepted);

    REQUIRE(clientLocal);
    REQUIRE(clientPeer);
    REQUIRE(acceptedLocal);
    REQUIRE(acceptedPeer);

    REQUIRE(clientPeer.value().toIp() == serverAddr.toIp());
    REQUIRE(clientPeer.value().port() == serverAddr.port());

    REQUIRE(acceptedLocal.value().toIp() == serverAddr.toIp());
    REQUIRE(acceptedLocal.value().port() == serverAddr.port());

    REQUIRE(acceptedPeer.value().toIp() == clientLocal.value().toIp());
    REQUIRE(acceptedPeer.value().port() == clientLocal.value().port());

    closeIfValid(accepted);
    closeIfValid(client);
    closeIfValid(server);
}

TEST_CASE("SocketOps write and read transfer bytes across connected sockets", "[net][socket_ops]")
{
    const SocketType server = SocketOps::createTcpNonblockingOrDie(AF_INET);
    REQUIRE(SocketOps::setReuseAddr(server, true));
    REQUIRE(SocketOps::bind(server, ipv4Loopback(0)));
    REQUIRE(SocketOps::listen(server));

    const InetAddress serverAddr = SocketOps::localAddress(server).value();
    const SocketType client = connectBlocking(serverAddr);

    InetAddress peerAddr;
    SocketType accepted = kInvalidSocket;

    for (int i = 0; i < 30 && accepted == kInvalidSocket; ++i)
    {
        auto acceptRet = SocketOps::accept(server, &peerAddr);
        if (acceptRet)
        {
            accepted = acceptRet.value();
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    REQUIRE(accepted != kInvalidSocket);

    const char message[] = "hello";
    const int written = SocketOps::write(client, message, 5);
    REQUIRE(written == 5);

    char buffer[16]{};
    int readBytes = -1;

    for (int i = 0; i < 30 && readBytes < 0; ++i)
    {
        readBytes = SocketOps::read(accepted, buffer, sizeof(buffer));
        if (readBytes < 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    REQUIRE(readBytes == 5);
    REQUIRE(std::string(buffer, buffer + 5) == "hello");

    closeIfValid(accepted);
    closeIfValid(client);
    closeIfValid(server);
}

TEST_CASE("SocketOps shutdownWrite succeeds on connected socket", "[net][socket_ops]")
{
    const SocketType server = SocketOps::createTcpNonblockingOrDie(AF_INET);
    REQUIRE(SocketOps::setReuseAddr(server, true));
    REQUIRE(SocketOps::bind(server, ipv4Loopback(0)));
    REQUIRE(SocketOps::listen(server));

    const InetAddress serverAddr = SocketOps::localAddress(server).value();
    const SocketType client = connectBlocking(serverAddr);

    InetAddress peerAddr;
    SocketType accepted = kInvalidSocket;

    for (int i = 0; i < 30 && accepted == kInvalidSocket; ++i)
    {
        auto acceptRet = SocketOps::accept(server, &peerAddr);
        if (acceptRet)
        {
            accepted = acceptRet.value();
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    REQUIRE(accepted != kInvalidSocket);
    REQUIRE_NOTHROW(SocketOps::shutdownWrite(client));

    closeIfValid(accepted);
    closeIfValid(client);
    closeIfValid(server);
}

TEST_CASE("SocketOps setReuseAddr succeeds on valid socket", "[net][socket_ops]")
{
    const SocketType sock = SocketOps::createTcpNonblockingOrDie(AF_INET);

    REQUIRE(SocketOps::setReuseAddr(sock, true));
    REQUIRE(SocketOps::setReuseAddr(sock, false));

    closeIfValid(sock);
}

TEST_CASE("SocketOps setReusePort is platform dependent", "[net][socket_ops]")
{
    const SocketType sock = SocketOps::createTcpNonblockingOrDie(AF_INET);

#if defined(_WIN32)
    auto ret = SocketOps::setReusePort(sock, true);
    REQUIRE_FALSE(ret);
    REQUIRE_THAT(ret.error().message(), Catch::Matchers::ContainsSubstring("SO_REUSEPORT"));
#else
    REQUIRE(SocketOps::setReusePort(sock, true));
    REQUIRE(SocketOps::setReusePort(sock, false));
#endif

    closeIfValid(sock);
}

TEST_CASE("SocketOps setTcpNoDelay succeeds on TCP socket", "[net][socket_ops]")
{
    const SocketType sock = SocketOps::createTcpNonblockingOrDie(AF_INET);

    REQUIRE(SocketOps::setTcpNoDelay(sock, true));
    REQUIRE(SocketOps::setTcpNoDelay(sock, false));

    closeIfValid(sock);
}

TEST_CASE("SocketOps setKeepAlive succeeds on socket", "[net][socket_ops]")
{
    const SocketType sock = SocketOps::createTcpNonblockingOrDie(AF_INET);

    REQUIRE(SocketOps::setKeepAlive(sock, true));
    REQUIRE(SocketOps::setKeepAlive(sock, false));

    closeIfValid(sock);
}

TEST_CASE("SocketOps setNonBlock succeeds toggling mode", "[net][socket_ops]")
{
    const SocketType sock = SocketOps::createTcpNonblockingOrDie(AF_INET);

    REQUIRE(SocketOps::setNonBlock(sock, false));
    REQUIRE(SocketOps::setNonBlock(sock, true));

    closeIfValid(sock);
}

TEST_CASE("SocketOps setIpv6Only succeeds on IPv6 socket", "[net][socket_ops]")
{
    const SocketType sock = SocketOps::createTcpNonblockingOrDie(AF_INET6);

    REQUIRE(SocketOps::setIpv6Only(sock, true));
    REQUIRE(SocketOps::setIpv6Only(sock, false));

    closeIfValid(sock);
}

TEST_CASE("SocketOps getSocketError returns zero on fresh socket", "[net][socket_ops]")
{
    const SocketType sock = SocketOps::createTcpNonblockingOrDie(AF_INET);

    REQUIRE(SocketOps::getSocketError(sock) == 0);

    closeIfValid(sock);
}

TEST_CASE("SocketOps lastErrorMessage returns non empty message after failed operation", "[net][socket_ops]")
{
    InetAddress peerAddr;
    auto ret = SocketOps::accept(kInvalidSocket, &peerAddr);

    REQUIRE_FALSE(ret);
    REQUIRE_FALSE(SocketOps::lastErrorMessage().empty());
}

TEST_CASE("SocketOps bind and listen support IPv6 loopback when IPv6 is available", "[net][socket_ops]")
{
    const SocketType sock = SocketOps::createTcpNonblockingOrDie(AF_INET6);

    REQUIRE(SocketOps::setReuseAddr(sock, true));
    REQUIRE(SocketOps::setIpv6Only(sock, true));

    const auto bindRet = SocketOps::bind(sock, InetAddress(0, true, true));

    if (isLinuxIpv6Disabled())
    {
        REQUIRE_FALSE(bindRet);
        closeIfValid(sock);
        return;
    }

    REQUIRE(bindRet);
    REQUIRE(SocketOps::listen(sock));

    const auto local = SocketOps::localAddress(sock);
    REQUIRE(local);
    REQUIRE(local.value().isIpv6());
    REQUIRE(local.value().isLoopbackIp());
    REQUIRE(local.value().port() != 0);

    closeIfValid(sock);
}