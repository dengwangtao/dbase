#pragma once

#include "dbase/net/inet_address.h"
#include "dbase/net/socket.h"

#include <cstddef>
#include <functional>

namespace dbase::net
{
class Acceptor
{
    public:
        using NewConnectionCallback = std::function<void(Socket, const InetAddress&)>;

        Acceptor(const InetAddress& listenAddr, bool reusePort, bool ipv6Only);

        Acceptor(const Acceptor&) = delete;
        Acceptor& operator=(const Acceptor&) = delete;

        ~Acceptor() = default;

        void setNewConnectionCallback(NewConnectionCallback cb);

        void listen();

        [[nodiscard]] bool listening() const noexcept;
        [[nodiscard]] const InetAddress& listenAddress() const noexcept;
        [[nodiscard]] Socket& socket() noexcept;
        [[nodiscard]] const Socket& socket() const noexcept;

        void setEdgeTriggered(bool on) noexcept;
        [[nodiscard]] bool edgeTriggered() const noexcept;

        std::size_t acceptAvailable();

    private:
        Socket m_socket;
        InetAddress m_listenAddr;
        bool m_listening{false};
        bool m_edgeTriggered{false};
        NewConnectionCallback m_newConnectionCallback;
};
}  // namespace dbase::net