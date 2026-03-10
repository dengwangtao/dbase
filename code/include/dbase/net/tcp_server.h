#pragma once

#include "dbase/net/acceptor.h"
#include "dbase/net/event_loop.h"
#include "dbase/net/tcp_connection.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace dbase::net
{
class TcpServer
{
    public:
        using ConnectionCallback = TcpConnection::ConnectionCallback;
        using MessageCallback = TcpConnection::MessageCallback;
        using WriteCompleteCallback = TcpConnection::WriteCompleteCallback;

        TcpServer(
                EventLoop* loop,
                const InetAddress& listenAddr,
                std::string name,
                bool reusePort = false,
                bool ipv6Only = false);

        TcpServer(const TcpServer&) = delete;
        TcpServer& operator=(const TcpServer&) = delete;

        ~TcpServer();

        void setConnectionCallback(ConnectionCallback cb);
        void setMessageCallback(MessageCallback cb);
        void setWriteCompleteCallback(WriteCompleteCallback cb);

        void start();

        [[nodiscard]] EventLoop* ownerLoop() const noexcept;
        [[nodiscard]] const std::string& name() const noexcept;
        [[nodiscard]] bool started() const noexcept;
        [[nodiscard]] std::size_t connectionCount() const noexcept;

    private:
        void newConnection(Socket socket, const InetAddress& peerAddr);
        void removeConnection(const TcpConnection::Ptr& conn);
        void removeConnectionInLoop(const TcpConnection::Ptr& conn);

    private:
        EventLoop* m_loop{nullptr};
        std::string m_name;
        Acceptor m_acceptor;
        std::unique_ptr<Channel> m_acceptChannel;
        bool m_started{false};
        std::int64_t m_nextConnId{1};

        ConnectionCallback m_connectionCallback;
        MessageCallback m_messageCallback;
        WriteCompleteCallback m_writeCompleteCallback;

        std::unordered_map<std::string, TcpConnection::Ptr> m_connections;
};
}  // namespace dbase::net