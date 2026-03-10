#include "dbase/net/inet_address.h"

#include <cstring>
#include <stdexcept>

#if !defined(_WIN32)
#include <arpa/inet.h>
#include <netdb.h>
#endif

namespace dbase::net
{
InetAddress::InetAddress() noexcept
    : m_addr()
{
    std::memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin_family = AF_INET;
}

InetAddress::InetAddress(std::uint16_t port, bool loopbackOnly) noexcept
    : m_addr()
{
    std::memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin_family = AF_INET;
    m_addr.sin_port = htons(port);
    m_addr.sin_addr.s_addr = htonl(loopbackOnly ? INADDR_LOOPBACK : INADDR_ANY);
}

InetAddress::InetAddress(std::string ip, std::uint16_t port)
    : m_addr()
{
    std::memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin_family = AF_INET;
    m_addr.sin_port = htons(port);

    const int rc = ::inet_pton(AF_INET, ip.c_str(), &m_addr.sin_addr);
    if (rc <= 0)
    {
        throw std::invalid_argument("InetAddress invalid IPv4 address: " + ip);
    }
}

InetAddress::InetAddress(const sockaddr_in& addr) noexcept
    : m_addr(addr)
{
}

const sockaddr_in& InetAddress::getSockAddrInet() const noexcept
{
    return m_addr;
}

void InetAddress::setSockAddrInet(const sockaddr_in& addr) noexcept
{
    m_addr = addr;
}

std::string InetAddress::toIp() const
{
    char buf[64] = {0};
    const char* ret = ::inet_ntop(
            AF_INET,
            static_cast<const void*>(&m_addr.sin_addr),
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
    return toIp() + ":" + std::to_string(port());
}

std::uint16_t InetAddress::port() const noexcept
{
    return ntohs(m_addr.sin_port);
}

bool InetAddress::isLoopbackIp() const noexcept
{
    return ntohl(m_addr.sin_addr.s_addr) == INADDR_LOOPBACK;
}

bool InetAddress::isAnyIp() const noexcept
{
    return ntohl(m_addr.sin_addr.s_addr) == INADDR_ANY;
}

bool InetAddress::resolve(const std::string& hostname, InetAddress* out)
{
    if (out == nullptr)
    {
        return false;
    }

    addrinfo hints;
    std::memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    const int rc = ::getaddrinfo(hostname.c_str(), nullptr, &hints, &result);
    if (rc != 0 || result == nullptr)
    {
        return false;
    }

    const auto* addr = reinterpret_cast<sockaddr_in*>(result->ai_addr);
    out->setSockAddrInet(*addr);
    ::freeaddrinfo(result);
    return true;
}

}  // namespace dbase::net