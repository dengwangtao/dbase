#pragma once

#include "dbase/net/connector.h"
#include "dbase/net/event_loop.h"
#include "dbase/net/inet_address.h"
#include "dbase/net/tcp_connection.h"

#include <memory>
#include <mutex>
#include <string>

namespace dbase::net
{
class TcpClient
{
    public:
        using ConnectionCallback = TcpConnection::ConnectionCallback;
        using MessageCallback = TcpConnection::MessageCallback;
        using WriteCompleteCallback = TcpConnection::WriteCompleteCallback;

        TcpClient(EventLoop* loop, const InetAddress& serverAddr, std::string name);

        TcpClient(const TcpClient&) = delete;
        TcpClient& operator=(const TcpClient&) = delete;

        ~TcpClient();

        void setConnectionCallback(ConnectionCallback cb);
        void setMessageCallback(MessageCallback cb);
        void setWriteCompleteCallback(WriteCompleteCallback cb);

        void enableRetry(bool on) noexcept;

        void connect();
        void disconnect();
        void stop();

        [[nodiscard]] EventLoop* ownerLoop() const noexcept;
        [[nodiscard]] const std::string& name() const noexcept;
        [[nodiscard]] const InetAddress& serverAddress() const noexcept;
        [[nodiscard]] bool retryEnabled() const noexcept;

        [[nodiscard]] TcpConnection::Ptr connection() const;

    private:
        void newConnection(Socket socket);
        void removeConnection(const TcpConnection::Ptr& conn);

    private:
        EventLoop* m_loop{nullptr};
        std::string m_name;
        InetAddress m_serverAddr;
        std::shared_ptr<Connector> m_connector;
        ConnectionCallback m_connectionCallback;
        MessageCallback m_messageCallback;
        WriteCompleteCallback m_writeCompleteCallback;
        bool m_retry{false};

        mutable std::mutex m_mutex;
        TcpConnection::Ptr m_connection;
};
}  // namespace dbase::net