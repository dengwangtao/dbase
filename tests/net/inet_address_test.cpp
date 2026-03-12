#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <stdexcept>
#include <string>

#include "dbase/net/inet_address.h"
#include "dbase/net/socket_ops.h"

#if defined(_WIN32)
#include <WS2tcpip.h>
#else
#include <arpa/inet.h>
#endif

namespace
{
struct SocketOpsInit
{
        SocketOpsInit()
        {
            dbase::net::SocketOps::initialize();
        }
};

const SocketOpsInit kSocketOpsInit{};

using dbase::net::InetAddress;

sockaddr_in makeSockAddrIn(const char* ip, std::uint16_t port)
{
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    const int rc = ::inet_pton(AF_INET, ip, &addr.sin_addr);
    if (rc != 1)
    {
        throw std::runtime_error("inet_pton AF_INET failed");
    }
    return addr;
}

sockaddr_in6 makeSockAddrIn6(const char* ip, std::uint16_t port)
{
    sockaddr_in6 addr{};
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port);
    const int rc = ::inet_pton(AF_INET6, ip, &addr.sin6_addr);
    if (rc != 1)
    {
        throw std::runtime_error("inet_pton AF_INET6 failed");
    }
    return addr;
}
}  // namespace

TEST_CASE("InetAddress default constructor creates IPv4 any address with port zero", "[net][inet_address]")
{
    InetAddress addr;

    REQUIRE(addr.isIpv4());
    REQUIRE_FALSE(addr.isIpv6());
    REQUIRE(addr.addressFamily() == AF_INET);
    REQUIRE(addr.length() == static_cast<socklen_t>(sizeof(sockaddr_in)));
    REQUIRE(addr.port() == 0);
    REQUIRE(addr.isAnyIp());
    REQUIRE_FALSE(addr.isLoopbackIp());
    REQUIRE(addr.toIp() == "0.0.0.0");
    REQUIRE(addr.toIpPort() == "0.0.0.0:0");
    REQUIRE(addr.getSockAddr() != nullptr);
    REQUIRE(addr.getSockAddrInet() != nullptr);
    REQUIRE(addr.getSockAddrInet6() == nullptr);
}

TEST_CASE("InetAddress port constructor creates IPv4 any address by default", "[net][inet_address]")
{
    InetAddress addr(8080);

    REQUIRE(addr.isIpv4());
    REQUIRE_FALSE(addr.isIpv6());
    REQUIRE(addr.addressFamily() == AF_INET);
    REQUIRE(addr.port() == 8080);
    REQUIRE(addr.isAnyIp());
    REQUIRE_FALSE(addr.isLoopbackIp());
    REQUIRE(addr.toIp() == "0.0.0.0");
    REQUIRE(addr.toIpPort() == "0.0.0.0:8080");
}

TEST_CASE("InetAddress port constructor supports IPv4 loopback", "[net][inet_address]")
{
    InetAddress addr(9527, true, false);

    REQUIRE(addr.isIpv4());
    REQUIRE_FALSE(addr.isIpv6());
    REQUIRE(addr.addressFamily() == AF_INET);
    REQUIRE(addr.port() == 9527);
    REQUIRE(addr.isLoopbackIp());
    REQUIRE_FALSE(addr.isAnyIp());
    REQUIRE(addr.toIp() == "127.0.0.1");
    REQUIRE(addr.toIpPort() == "127.0.0.1:9527");
}

TEST_CASE("InetAddress port constructor supports IPv6 any address", "[net][inet_address]")
{
    InetAddress addr(8081, false, true);

    REQUIRE_FALSE(addr.isIpv4());
    REQUIRE(addr.isIpv6());
    REQUIRE(addr.addressFamily() == AF_INET6);
    REQUIRE(addr.port() == 8081);
    REQUIRE(addr.isAnyIp());
    REQUIRE_FALSE(addr.isLoopbackIp());
    REQUIRE(addr.toIp() == "::");
    REQUIRE(addr.toIpPort() == "[::]:8081");
    REQUIRE(addr.getSockAddrInet() == nullptr);
    REQUIRE(addr.getSockAddrInet6() != nullptr);
}

TEST_CASE("InetAddress port constructor supports IPv6 loopback", "[net][inet_address]")
{
    InetAddress addr(8082, true, true);

    REQUIRE_FALSE(addr.isIpv4());
    REQUIRE(addr.isIpv6());
    REQUIRE(addr.addressFamily() == AF_INET6);
    REQUIRE(addr.port() == 8082);
    REQUIRE(addr.isLoopbackIp());
    REQUIRE_FALSE(addr.isAnyIp());
    REQUIRE(addr.toIp() == "::1");
    REQUIRE(addr.toIpPort() == "[::1]:8082");
}

TEST_CASE("InetAddress string constructor supports IPv4 literal", "[net][inet_address]")
{
    InetAddress addr("192.168.1.10", 12345);

    REQUIRE(addr.isIpv4());
    REQUIRE_FALSE(addr.isIpv6());
    REQUIRE(addr.addressFamily() == AF_INET);
    REQUIRE(addr.port() == 12345);
    REQUIRE_FALSE(addr.isAnyIp());
    REQUIRE_FALSE(addr.isLoopbackIp());
    REQUIRE(addr.toIp() == "192.168.1.10");
    REQUIRE(addr.toIpPort() == "192.168.1.10:12345");
}

TEST_CASE("InetAddress string constructor supports IPv6 literal", "[net][inet_address]")
{
    InetAddress addr("::1", 54321);

    REQUIRE_FALSE(addr.isIpv4());
    REQUIRE(addr.isIpv6());
    REQUIRE(addr.addressFamily() == AF_INET6);
    REQUIRE(addr.port() == 54321);
    REQUIRE(addr.isLoopbackIp());
    REQUIRE_FALSE(addr.isAnyIp());
    REQUIRE(addr.toIp() == "::1");
    REQUIRE(addr.toIpPort() == "[::1]:54321");
}

TEST_CASE("InetAddress sockaddr_in constructor preserves IPv4 address and port", "[net][inet_address]")
{
    const sockaddr_in raw = makeSockAddrIn("10.20.30.40", 6000);

    InetAddress addr(raw);

    REQUIRE(addr.isIpv4());
    REQUIRE_FALSE(addr.isIpv6());
    REQUIRE(addr.addressFamily() == AF_INET);
    REQUIRE(addr.length() == static_cast<socklen_t>(sizeof(sockaddr_in)));
    REQUIRE(addr.port() == 6000);
    REQUIRE(addr.toIp() == "10.20.30.40");
    REQUIRE(addr.toIpPort() == "10.20.30.40:6000");
}

TEST_CASE("InetAddress sockaddr_in6 constructor preserves IPv6 address and port", "[net][inet_address]")
{
    const sockaddr_in6 raw = makeSockAddrIn6("2001:db8::1", 6001);

    InetAddress addr(raw);

    REQUIRE_FALSE(addr.isIpv4());
    REQUIRE(addr.isIpv6());
    REQUIRE(addr.addressFamily() == AF_INET6);
    REQUIRE(addr.length() == static_cast<socklen_t>(sizeof(sockaddr_in6)));
    REQUIRE(addr.port() == 6001);
    REQUIRE(addr.toIp() == "2001:db8::1");
    REQUIRE(addr.toIpPort() == "[2001:db8::1]:6001");
}

TEST_CASE("InetAddress sockaddr pointer constructor supports IPv4", "[net][inet_address]")
{
    const sockaddr_in raw = makeSockAddrIn("172.16.0.8", 7000);

    InetAddress addr(reinterpret_cast<const sockaddr*>(&raw), static_cast<socklen_t>(sizeof(raw)));

    REQUIRE(addr.isIpv4());
    REQUIRE(addr.addressFamily() == AF_INET);
    REQUIRE(addr.port() == 7000);
    REQUIRE(addr.toIp() == "172.16.0.8");
    REQUIRE(addr.toIpPort() == "172.16.0.8:7000");
}

TEST_CASE("InetAddress sockaddr pointer constructor supports IPv6", "[net][inet_address]")
{
    const sockaddr_in6 raw = makeSockAddrIn6("fe80::1", 7001);

    InetAddress addr(reinterpret_cast<const sockaddr*>(&raw), static_cast<socklen_t>(sizeof(raw)));

    REQUIRE(addr.isIpv6());
    REQUIRE(addr.addressFamily() == AF_INET6);
    REQUIRE(addr.port() == 7001);
    REQUIRE(addr.toIp() == "fe80::1");
    REQUIRE(addr.toIpPort() == "[fe80::1]:7001");
}

TEST_CASE("InetAddress setSockAddrInet replaces address with IPv4 sockaddr_in", "[net][inet_address]")
{
    InetAddress addr(1234, true, true);
    const sockaddr_in raw = makeSockAddrIn("8.8.8.8", 53);

    addr.setSockAddrInet(raw);

    REQUIRE(addr.isIpv4());
    REQUIRE_FALSE(addr.isIpv6());
    REQUIRE(addr.addressFamily() == AF_INET);
    REQUIRE(addr.length() == static_cast<socklen_t>(sizeof(sockaddr_in)));
    REQUIRE(addr.port() == 53);
    REQUIRE(addr.toIp() == "8.8.8.8");
    REQUIRE(addr.toIpPort() == "8.8.8.8:53");
}

TEST_CASE("InetAddress setSockAddrInet6 replaces address with IPv6 sockaddr_in6", "[net][inet_address]")
{
    InetAddress addr(1234);
    const sockaddr_in6 raw = makeSockAddrIn6("2001:db8::2", 443);

    addr.setSockAddrInet6(raw);

    REQUIRE_FALSE(addr.isIpv4());
    REQUIRE(addr.isIpv6());
    REQUIRE(addr.addressFamily() == AF_INET6);
    REQUIRE(addr.length() == static_cast<socklen_t>(sizeof(sockaddr_in6)));
    REQUIRE(addr.port() == 443);
    REQUIRE(addr.toIp() == "2001:db8::2");
    REQUIRE(addr.toIpPort() == "[2001:db8::2]:443");
}

TEST_CASE("InetAddress setSockAddr with sockaddr pointer supports IPv4 and IPv6", "[net][inet_address]")
{
    InetAddress addr;

    const sockaddr_in raw4 = makeSockAddrIn("1.2.3.4", 1111);
    addr.setSockAddr(reinterpret_cast<const sockaddr*>(&raw4), static_cast<socklen_t>(sizeof(raw4)));

    REQUIRE(addr.isIpv4());
    REQUIRE(addr.port() == 1111);
    REQUIRE(addr.toIp() == "1.2.3.4");

    const sockaddr_in6 raw6 = makeSockAddrIn6("::1", 2222);
    addr.setSockAddr(reinterpret_cast<const sockaddr*>(&raw6), static_cast<socklen_t>(sizeof(raw6)));

    REQUIRE(addr.isIpv6());
    REQUIRE(addr.port() == 2222);
    REQUIRE(addr.toIp() == "::1");
    REQUIRE(addr.toIpPort() == "[::1]:2222");
}

TEST_CASE("InetAddress non const getSockAddr returns writable pointer to stored address", "[net][inet_address]")
{
    InetAddress addr("127.0.0.1", 9000);

    sockaddr* raw = addr.getSockAddr();

    REQUIRE(raw != nullptr);
    REQUIRE(raw->sa_family == AF_INET);
    REQUIRE(addr.getSockAddr() == raw);
}

TEST_CASE("InetAddress loopback and any detection works for both families", "[net][inet_address]")
{
    InetAddress ipv4Any(80, false, false);
    InetAddress ipv4Loopback(80, true, false);
    InetAddress ipv6Any(80, false, true);
    InetAddress ipv6Loopback(80, true, true);

    REQUIRE(ipv4Any.isAnyIp());
    REQUIRE_FALSE(ipv4Any.isLoopbackIp());

    REQUIRE_FALSE(ipv4Loopback.isAnyIp());
    REQUIRE(ipv4Loopback.isLoopbackIp());

    REQUIRE(ipv6Any.isAnyIp());
    REQUIRE_FALSE(ipv6Any.isLoopbackIp());

    REQUIRE_FALSE(ipv6Loopback.isAnyIp());
    REQUIRE(ipv6Loopback.isLoopbackIp());
}

TEST_CASE("InetAddress resolve rejects null output pointer", "[net][inet_address]")
{
    REQUIRE_FALSE(InetAddress::resolve("localhost", nullptr, false));
}

TEST_CASE("InetAddress resolve can resolve localhost", "[net][inet_address]")
{
    InetAddress addr;

    const bool ok = InetAddress::resolve("localhost", &addr, false);

    REQUIRE(ok);
    REQUIRE((addr.isIpv4() || addr.isIpv6()));
    REQUIRE((addr.addressFamily() == AF_INET || addr.addressFamily() == AF_INET6));
    REQUIRE_FALSE(addr.toIp().empty());
    REQUIRE(addr.port() == 0);
}

TEST_CASE("InetAddress resolve can prefer IPv6 when available", "[net][inet_address]")
{
    InetAddress addr;

    const bool ok = InetAddress::resolve("localhost", &addr, true);

    REQUIRE(ok);
    REQUIRE((addr.isIpv4() || addr.isIpv6()));
    REQUIRE_FALSE(addr.toIp().empty());
    REQUIRE(addr.port() == 0);
}

TEST_CASE("InetAddress resolve returns false for invalid host", "[net][inet_address]")
{
    InetAddress addr;

    REQUIRE_FALSE(InetAddress::resolve("nonexistent-host-for-dbase-test.invalid", &addr, false));
}