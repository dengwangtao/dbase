#include "dbase/net/inet_address.h"

#include <cstring>
#include <stdexcept>

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <netdb.h>
#endif

namespace dbase::net
{
namespace
{
[[nodiscard]] const void* addrData(const sockaddr_storage& storage) noexcept
{
    const auto family = storage.ss_family;
    if (family == AF_INET)
    {
        return &reinterpret_cast<const sockaddr_in*>(&storage)->sin_addr;
    }

    if (family == AF_INET6)
    {
        return &reinterpret_cast<const sockaddr_in6*>(&storage)->sin6_addr;
    }

    return nullptr;
}
}  // namespace

InetAddress::InetAddress() noexcept
    : m_addr(),
      m_length(static_cast<socklen_t>(sizeof(sockaddr_in)))
{
    std::memset(&m_addr, 0, sizeof(m_addr));
    auto* addr = reinterpret_cast<sockaddr_in*>(&m_addr);
    addr->sin_family = AF_INET;
}

InetAddress::InetAddress(std::uint16_t port, bool loopbackOnly, bool ipv6) noexcept
    : m_addr(),
      m_length(static_cast<socklen_t>(ipv6 ? sizeof(sockaddr_in6) : sizeof(sockaddr_in)))
{
    std::memset(&m_addr, 0, sizeof(m_addr));

    if (ipv6)
    {
        auto* addr6 = reinterpret_cast<sockaddr_in6*>(&m_addr);
        addr6->sin6_family = AF_INET6;
        addr6->sin6_port = htons(port);
        addr6->sin6_addr = loopbackOnly ? in6addr_loopback : in6addr_any;
    }
    else
    {
        auto* addr = reinterpret_cast<sockaddr_in*>(&m_addr);
        addr->sin_family = AF_INET;
        addr->sin_port = htons(port);
        addr->sin_addr.s_addr = htonl(loopbackOnly ? INADDR_LOOPBACK : INADDR_ANY);
    }
}

InetAddress::InetAddress(std::string ip, std::uint16_t port)
    : m_addr(),
      m_length(0)
{
    std::memset(&m_addr, 0, sizeof(m_addr));

    sockaddr_in addr4;
    std::memset(&addr4, 0, sizeof(addr4));
    addr4.sin_family = AF_INET;
    addr4.sin_port = htons(port);

    if (::inet_pton(AF_INET, ip.c_str(), &addr4.sin_addr) == 1)
    {
        setSockAddrInet(addr4);
        return;
    }

    sockaddr_in6 addr6;
    std::memset(&addr6, 0, sizeof(addr6));
    addr6.sin6_family = AF_INET6;
    addr6.sin6_port = htons(port);

    if (::inet_pton(AF_INET6, ip.c_str(), &addr6.sin6_addr) == 1)
    {
        setSockAddrInet6(addr6);
        return;
    }

    throw std::invalid_argument("InetAddress invalid IP address: " + ip);
}

InetAddress::InetAddress(const sockaddr* addr, socklen_t len)
    : m_addr(),
      m_length(0)
{
    setSockAddr(addr, len);
}

InetAddress::InetAddress(const sockaddr_in& addr) noexcept
    : m_addr(),
      m_length(static_cast<socklen_t>(sizeof(sockaddr_in)))
{
    std::memset(&m_addr, 0, sizeof(m_addr));
    std::memcpy(&m_addr, &addr, sizeof(addr));
}

InetAddress::InetAddress(const sockaddr_in6& addr) noexcept
    : m_addr(),
      m_length(static_cast<socklen_t>(sizeof(sockaddr_in6)))
{
    std::memset(&m_addr, 0, sizeof(m_addr));
    std::memcpy(&m_addr, &addr, sizeof(addr));
}

DbaseAddressFamily InetAddress::addressFamily() const noexcept
{
    return static_cast<DbaseAddressFamily>(m_addr.ss_family);
}

bool InetAddress::isIpv4() const noexcept
{
    return addressFamily() == AF_INET;
}

bool InetAddress::isIpv6() const noexcept
{
    return addressFamily() == AF_INET6;
}

const sockaddr* InetAddress::getSockAddr() const noexcept
{
    return reinterpret_cast<const sockaddr*>(&m_addr);
}

sockaddr* InetAddress::getSockAddr() noexcept
{
    return reinterpret_cast<sockaddr*>(&m_addr);
}

socklen_t InetAddress::length() const noexcept
{
    return m_length;
}

const sockaddr_in* InetAddress::getSockAddrInet() const noexcept
{
    return isIpv4() ? reinterpret_cast<const sockaddr_in*>(&m_addr) : nullptr;
}

const sockaddr_in6* InetAddress::getSockAddrInet6() const noexcept
{
    return isIpv6() ? reinterpret_cast<const sockaddr_in6*>(&m_addr) : nullptr;
}

void InetAddress::setSockAddr(const sockaddr* addr, socklen_t len)
{
    if (addr == nullptr)
    {
        throw std::invalid_argument("InetAddress::setSockAddr addr is null");
    }

    if (len != sizeof(sockaddr_in) && len != sizeof(sockaddr_in6))
    {
        throw std::invalid_argument("InetAddress::setSockAddr invalid address length");
    }

    std::memset(&m_addr, 0, sizeof(m_addr));
    std::memcpy(&m_addr, addr, len);
    m_length = len;
}

void InetAddress::setSockAddrInet(const sockaddr_in& addr) noexcept
{
    std::memset(&m_addr, 0, sizeof(m_addr));
    std::memcpy(&m_addr, &addr, sizeof(addr));
    m_length = static_cast<socklen_t>(sizeof(addr));
}

void InetAddress::setSockAddrInet6(const sockaddr_in6& addr) noexcept
{
    std::memset(&m_addr, 0, sizeof(m_addr));
    std::memcpy(&m_addr, &addr, sizeof(addr));
    m_length = static_cast<socklen_t>(sizeof(addr));
}

std::string InetAddress::toIp() const
{
    char buf[INET6_ADDRSTRLEN] = {0};
    const void* data = addrData(m_addr);
    if (data == nullptr)
    {
        return {};
    }

    const char* ret = ::inet_ntop(
            addressFamily(),
            data,
            buf,
            static_cast<socklen_t>(sizeof(buf)));
    if (ret == nullptr)
    {
        return {};
    }

    return std::string(buf);
}

std::string InetAddress::toIpPort() const
{
    if (isIpv6())
    {
        return "[" + toIp() + "]:" + std::to_string(port());
    }

    return toIp() + ":" + std::to_string(port());
}

std::uint16_t InetAddress::port() const noexcept
{
    if (isIpv4())
    {
        return ntohs(reinterpret_cast<const sockaddr_in*>(&m_addr)->sin_port);
    }

    if (isIpv6())
    {
        return ntohs(reinterpret_cast<const sockaddr_in6*>(&m_addr)->sin6_port);
    }

    return 0;
}

bool InetAddress::isLoopbackIp() const noexcept
{
    if (isIpv4())
    {
        return ntohl(reinterpret_cast<const sockaddr_in*>(&m_addr)->sin_addr.s_addr) == INADDR_LOOPBACK;
    }

    if (isIpv6())
    {
        const auto* addr6 = reinterpret_cast<const sockaddr_in6*>(&m_addr);
        return IN6_IS_ADDR_LOOPBACK(&addr6->sin6_addr) != 0;
    }

    return false;
}

bool InetAddress::isAnyIp() const noexcept
{
    if (isIpv4())
    {
        return ntohl(reinterpret_cast<const sockaddr_in*>(&m_addr)->sin_addr.s_addr) == INADDR_ANY;
    }

    if (isIpv6())
    {
        const auto* addr6 = reinterpret_cast<const sockaddr_in6*>(&m_addr);
        return IN6_IS_ADDR_UNSPECIFIED(&addr6->sin6_addr) != 0;
    }

    return false;
}

bool InetAddress::resolve(const std::string& hostname, InetAddress* out, bool preferIpv6)
{
    if (out == nullptr)
    {
        return false;
    }

    addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    const int rc = ::getaddrinfo(hostname.c_str(), nullptr, &hints, &result);
    if (rc != 0 || result == nullptr)
    {
        return false;
    }

    const addrinfo* chosen = nullptr;

    if (preferIpv6)
    {
        for (auto* p = result; p != nullptr; p = p->ai_next)
        {
            if (p->ai_family == AF_INET6)
            {
                chosen = p;
                break;
            }
        }
    }

    if (chosen == nullptr)
    {
        for (auto* p = result; p != nullptr; p = p->ai_next)
        {
            if (p->ai_family == AF_INET || p->ai_family == AF_INET6)
            {
                chosen = p;
                break;
            }
        }
    }

    if (chosen != nullptr)
    {
        out->setSockAddr(chosen->ai_addr, static_cast<socklen_t>(chosen->ai_addrlen));
        ::freeaddrinfo(result);
        return true;
    }

    ::freeaddrinfo(result);
    return false;
}

}  // namespace dbase::net