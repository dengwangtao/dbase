#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "dbase/net/acceptor.h"
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

using dbase::net::Acceptor;
using dbase::net::InetAddress;
using dbase::net::Socket;
using dbase::net::SocketOps;
using dbase::net::SocketType;

[[nodiscard]] InetAddress loopbackListenAddress(std::uint16_t port = 0)
{
    return InetAddress(port, true, false);
}

[[nodiscard]] Socket connectClient(const InetAddress& addr)
{
    Socket client = Socket::createTcp(addr.addressFamily());
    SocketOps::setNonBlock(client.fd(), false);
    auto ret = SocketOps::connect(client.fd(), addr);
    if (!ret)
    {
        throw std::runtime_error("connect failed: " + ret.error().message());
    }
    return client;
}
}  // namespace

TEST_CASE("Acceptor default state after construction", "[net][acceptor]")
{
    const InetAddress listenAddr = loopbackListenAddress(0);
    Acceptor acceptor(listenAddr, false, false);

    REQUIRE_FALSE(acceptor.listening());
    REQUIRE(acceptor.listenAddress().isIpv4());
    REQUIRE(acceptor.listenAddress().isLoopbackIp());
    REQUIRE(acceptor.listenAddress().port() == 0);
    REQUIRE_FALSE(acceptor.edgeTriggered());
    REQUIRE(acceptor.socket().valid());
    REQUIRE(acceptor.socket().fd() != dbase::net::kInvalidSocket);
}

TEST_CASE("Acceptor listen is idempotent", "[net][acceptor]")
{
    const InetAddress listenAddr = loopbackListenAddress(0);
    Acceptor acceptor(listenAddr, false, false);

    REQUIRE_FALSE(acceptor.listening());

    acceptor.listen();
    REQUIRE(acceptor.listening());

    REQUIRE_NOTHROW(acceptor.listen());
    REQUIRE(acceptor.listening());
}

TEST_CASE("Acceptor acceptAvailable before listen throws", "[net][acceptor]")
{
    const InetAddress listenAddr = loopbackListenAddress(0);
    Acceptor acceptor(listenAddr, false, false);

    REQUIRE_THROWS_AS(acceptor.acceptAvailable(), std::logic_error);
}

TEST_CASE("Acceptor setNewConnectionCallback accepts empty and non empty callbacks", "[net][acceptor]")
{
    const InetAddress listenAddr = loopbackListenAddress(0);
    Acceptor acceptor(listenAddr, false, false);

    REQUIRE_NOTHROW(acceptor.setNewConnectionCallback({}));

    std::atomic<int> count{0};
    REQUIRE_NOTHROW(acceptor.setNewConnectionCallback(
            [&count](Socket, const InetAddress&)
            {
                count.fetch_add(1, std::memory_order_relaxed);
            }));
}

TEST_CASE("Acceptor setEdgeTriggered reflects platform behavior", "[net][acceptor]")
{
    const InetAddress listenAddr = loopbackListenAddress(0);
    Acceptor acceptor(listenAddr, false, false);

    REQUIRE_FALSE(acceptor.edgeTriggered());

    acceptor.setEdgeTriggered(true);

#if defined(__linux__)
    REQUIRE(acceptor.edgeTriggered());
#else
    REQUIRE_FALSE(acceptor.edgeTriggered());
#endif

    acceptor.setEdgeTriggered(false);
    REQUIRE_FALSE(acceptor.edgeTriggered());
}

TEST_CASE("Acceptor listens on bound address and reports local port", "[net][acceptor]")
{
    const InetAddress listenAddr = loopbackListenAddress(0);
    Acceptor acceptor(listenAddr, false, false);

    acceptor.listen();

    REQUIRE(acceptor.listening());

    const InetAddress local = acceptor.socket().localAddress();
    REQUIRE(local.isIpv4());
    REQUIRE(local.port() != 0);
}

TEST_CASE("Acceptor acceptAvailable returns zero when there are no pending clients", "[net][acceptor]")
{
    const InetAddress listenAddr = loopbackListenAddress(0);
    Acceptor acceptor(listenAddr, false, false);

    acceptor.listen();

    const std::size_t accepted = acceptor.acceptAvailable();

    REQUIRE(accepted == 0);
}

TEST_CASE("Acceptor acceptAvailable accepts one pending connection and invokes callback", "[net][acceptor]")
{
    const InetAddress listenAddr = loopbackListenAddress(0);
    Acceptor acceptor(listenAddr, false, false);
    acceptor.listen();

    const InetAddress serverAddr = acceptor.socket().localAddress();

    std::promise<InetAddress> peerPromise;
    auto peerFuture = peerPromise.get_future();
    std::atomic<int> callbackCount{0};

    acceptor.setNewConnectionCallback(
            [&callbackCount, &peerPromise](Socket socket, const InetAddress& peerAddr)
            {
                callbackCount.fetch_add(1, std::memory_order_relaxed);
                peerPromise.set_value(peerAddr);
                REQUIRE(socket.valid());
            });

    Socket client = connectClient(serverAddr);

    std::size_t accepted = 0;
    for (int i = 0; i < 20 && accepted == 0; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        accepted = acceptor.acceptAvailable();
    }

    REQUIRE(accepted == 1);
    REQUIRE(callbackCount.load(std::memory_order_relaxed) == 1);
    REQUIRE(peerFuture.wait_for(std::chrono::milliseconds(200)) == std::future_status::ready);

    const InetAddress peer = peerFuture.get();
    REQUIRE(peer.isIpv4());
    REQUIRE(peer.isLoopbackIp());
    REQUIRE(peer.port() != 0);
}

TEST_CASE("Acceptor acceptAvailable can accept multiple pending clients in one call", "[net][acceptor]")
{
    const InetAddress listenAddr = loopbackListenAddress(0);
    Acceptor acceptor(listenAddr, false, false);
    acceptor.listen();

    const InetAddress serverAddr = acceptor.socket().localAddress();

    std::atomic<int> callbackCount{0};
    std::vector<std::uint16_t> acceptedPorts;
    std::mutex acceptedMutex;

    acceptor.setNewConnectionCallback(
            [&callbackCount, &acceptedPorts, &acceptedMutex](Socket socket, const InetAddress& peerAddr)
            {
                REQUIRE(socket.valid());
                callbackCount.fetch_add(1, std::memory_order_relaxed);
                std::lock_guard<std::mutex> lock(acceptedMutex);
                acceptedPorts.push_back(peerAddr.port());
            });

    std::vector<Socket> clients;
    clients.reserve(3);
    for (int i = 0; i < 3; ++i)
    {
        clients.emplace_back(connectClient(serverAddr));
    }

    std::size_t accepted = 0;
    for (int i = 0; i < 30 && accepted < 3; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        accepted = acceptor.acceptAvailable();
    }

    REQUIRE(accepted == 3);
    REQUIRE(callbackCount.load(std::memory_order_relaxed) == 3);

    std::lock_guard<std::mutex> lock(acceptedMutex);
    REQUIRE(acceptedPorts.size() == 3);
    REQUIRE(acceptedPorts[0] != 0);
    REQUIRE(acceptedPorts[1] != 0);
    REQUIRE(acceptedPorts[2] != 0);
}

TEST_CASE("Acceptor acceptAvailable without callback still drains pending connections", "[net][acceptor]")
{
    const InetAddress listenAddr = loopbackListenAddress(0);
    Acceptor acceptor(listenAddr, false, false);
    acceptor.listen();

    const InetAddress serverAddr = acceptor.socket().localAddress();

    Socket client = connectClient(serverAddr);

    std::size_t accepted = 0;
    for (int i = 0; i < 20 && accepted == 0; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        accepted = acceptor.acceptAvailable();
    }

    REQUIRE(accepted == 1);
    REQUIRE(acceptor.acceptAvailable() == 0);
}

TEST_CASE("Acceptor listenAddress returns construction address object", "[net][acceptor]")
{
    const InetAddress listenAddr(34567, true, false);
    Acceptor acceptor(listenAddr, false, false);

    REQUIRE(acceptor.listenAddress().isIpv4());
    REQUIRE(acceptor.listenAddress().isLoopbackIp());
    REQUIRE(acceptor.listenAddress().port() == 34567);
}

TEST_CASE("Acceptor socket accessor returns same underlying socket object", "[net][acceptor]")
{
    const InetAddress listenAddr = loopbackListenAddress(0);
    Acceptor acceptor(listenAddr, false, false);

    Socket& sock1 = acceptor.socket();
    Socket& sock2 = acceptor.socket();
    const Socket& sock3 = std::as_const(acceptor).socket();

    REQUIRE(sock1.valid());
    REQUIRE(sock1.fd() == sock2.fd());
    REQUIRE(sock1.fd() == sock3.fd());
}

TEST_CASE("Acceptor supports IPv6 any-address listen construction", "[net][acceptor]")
{
    try
    {
        dbase::net::Socket probe = dbase::net::Socket::createTcp(AF_INET6);
        probe.setReuseAddr(true);
        probe.setIpv6Only(true);
        probe.bindAddress(dbase::net::InetAddress(0, false, true));
    }
    catch (...)
    {
        SKIP("IPv6 is not available in this environment");
    }

    const InetAddress listenAddr(0, false, true);
    Acceptor acceptor(listenAddr, false, true);

    REQUIRE(acceptor.listenAddress().isIpv6());
    REQUIRE_FALSE(acceptor.listenAddress().isLoopbackIp());
    REQUIRE_FALSE(acceptor.edgeTriggered());

    acceptor.listen();
    REQUIRE(acceptor.listening());

    const InetAddress local = acceptor.socket().localAddress();
    REQUIRE(local.isIpv6());
    REQUIRE(local.port() != 0);
}