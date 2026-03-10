#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#if defined(_WIN32)
#include <WinSock2.h>
#include <WS2tcpip.h>
using DbaseAddressFamily = ADDRESS_FAMILY;
#else
#include <netinet/in.h>
#include <sys/socket.h>
using DbaseAddressFamily = sa_family_t;
#endif

namespace dbase::net
{
class InetAddress
{
    public:
        InetAddress() noexcept;

        explicit InetAddress(
                std::uint16_t port,
                bool loopbackOnly = false,
                bool ipv6 = false) noexcept;

        InetAddress(std::string ip, std::uint16_t port);
        InetAddress(const sockaddr* addr, socklen_t len);
        explicit InetAddress(const sockaddr_in& addr) noexcept;
        explicit InetAddress(const sockaddr_in6& addr) noexcept;

        [[nodiscard]] DbaseAddressFamily addressFamily() const noexcept;
        [[nodiscard]] bool isIpv4() const noexcept;
        [[nodiscard]] bool isIpv6() const noexcept;

        [[nodiscard]] const sockaddr* getSockAddr() const noexcept;
        [[nodiscard]] sockaddr* getSockAddr() noexcept;
        [[nodiscard]] socklen_t length() const noexcept;

        [[nodiscard]] const sockaddr_in* getSockAddrInet() const noexcept;
        [[nodiscard]] const sockaddr_in6* getSockAddrInet6() const noexcept;

        void setSockAddr(const sockaddr* addr, socklen_t len);
        void setSockAddrInet(const sockaddr_in& addr) noexcept;
        void setSockAddrInet6(const sockaddr_in6& addr) noexcept;

        [[nodiscard]] std::string toIp() const;
        [[nodiscard]] std::string toIpPort() const;
        [[nodiscard]] std::uint16_t port() const noexcept;

        [[nodiscard]] bool isLoopbackIp() const noexcept;
        [[nodiscard]] bool isAnyIp() const noexcept;

        static bool resolve(
                const std::string& hostname,
                InetAddress* out,
                bool preferIpv6 = false);

    private:
        sockaddr_storage m_addr;
        socklen_t m_length;
};
}  // namespace dbase::net