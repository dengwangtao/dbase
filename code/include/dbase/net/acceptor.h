#pragma once

#include "dbase/net/inet_address.h"
#include "dbase/net/socket.h"

#include <functional>
#include <optional>

namespace dbase::net
{
class Acceptor
{
    public:
        using NewConnectionCallback = std::function<void(Socket, const InetAddress&)>;

        Acceptor(
                const InetAddress& listenAddr,
                bool reusePort = false,
                bool ipv6Only = false);

        Acceptor(const Acceptor&) = delete;
        Acceptor& operator=(const Acceptor&) = delete;

        Acceptor(Acceptor&&) = delete;
        Acceptor& operator=(Acceptor&&) = delete;

        ~Acceptor() = default;

        void setNewConnectionCallback(NewConnectionCallback cb);

        void listen(int backlog = SOMAXCONN);

        [[nodiscard]] bool listening() const noexcept;
        [[nodiscard]] const InetAddress& listenAddress() const noexcept;
        [[nodiscard]] const Socket& socket() const noexcept;

        [[nodiscard]] std::optional<std::pair<Socket, InetAddress>> acceptOnce();
        std::size_t acceptAvailable(std::size_t maxAcceptCount = 64);

    private:
        InetAddress m_listenAddr;
        Socket m_acceptSocket;
        bool m_listening{false};
        NewConnectionCallback m_newConnectionCallback;
};
}  // namespace dbase::net