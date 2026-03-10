#pragma once

#include <cstdint>
#include <string>

#if defined(_WIN32)
#include <WinSock2.h>
#include <WS2tcpip.h>
#else
#include <netinet/in.h>
#include <sys/socket.h>
#endif

namespace dbase::net
{
class InetAddress
{
    public:
        InetAddress() noexcept;
        explicit InetAddress(std::uint16_t port, bool loopbackOnly = false) noexcept;
        InetAddress(std::string ip, std::uint16_t port);
        explicit InetAddress(const sockaddr_in& addr) noexcept;

        [[nodiscard]] const sockaddr_in& getSockAddrInet() const noexcept;
        void setSockAddrInet(const sockaddr_in& addr) noexcept;

        [[nodiscard]] std::string toIp() const;
        [[nodiscard]] std::string toIpPort() const;
        [[nodiscard]] std::uint16_t port() const noexcept;

        [[nodiscard]] bool isLoopbackIp() const noexcept;
        [[nodiscard]] bool isAnyIp() const noexcept;

        static bool resolve(const std::string& hostname, InetAddress* out);

    private:
        sockaddr_in m_addr;
};
}  // namespace dbase::net