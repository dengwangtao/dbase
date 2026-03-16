#pragma once

#include "dbase/error/error.h"
#include "dbase/net/inet_address.h"
#include "dbase/net/socket.h"

#include <cstddef>
#include <span>

namespace dbase::net
{
struct UdpDatagram
{
        std::size_t size{0};
        InetAddress peer;
};

class UdpSocket
{
    public:
        UdpSocket() noexcept = default;
        explicit UdpSocket(Socket socket) noexcept;

        UdpSocket(const UdpSocket&) = delete;
        UdpSocket& operator=(const UdpSocket&) = delete;
        UdpSocket(UdpSocket&&) noexcept = default;
        UdpSocket& operator=(UdpSocket&&) noexcept = default;
        ~UdpSocket() = default;

        [[nodiscard]] static dbase::Result<UdpSocket> create(int family = AF_INET);

        [[nodiscard]] bool valid() const noexcept;
        [[nodiscard]] SocketType fd() const noexcept;

        void bindAddress(const InetAddress& addr);
        void connect(const InetAddress& addr);

        void setReuseAddr(bool on);
        void setReusePort(bool on);
        void setNonBlock(bool on);

        [[nodiscard]] dbase::Result<std::size_t> send(std::span<const std::byte> data);
        [[nodiscard]] dbase::Result<std::size_t> sendTo(std::span<const std::byte> data, const InetAddress& peer);

        [[nodiscard]] dbase::Result<UdpDatagram> receiveFrom(std::span<std::byte> buffer);
        [[nodiscard]] dbase::Result<std::size_t> receive(std::span<std::byte> buffer);

        [[nodiscard]] Socket& socket() noexcept;
        [[nodiscard]] const Socket& socket() const noexcept;

    private:
        Socket m_socket;
};
}  // namespace dbase::net