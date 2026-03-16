#pragma once
#include "dbase/net/connector.h"
#include "dbase/net/event_loop.h"
#include "dbase/net/inet_address.h"
#include "dbase/net/length_field_codec.h"
#include "dbase/net/tcp_connection.h"
#include <atomic>
#include <chrono>
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
        using FrameMessageCallback = TcpConnection::FrameMessageCallback;
        using WriteCompleteCallback = TcpConnection::WriteCompleteCallback;
        using HeartbeatCallback = std::function<void(const TcpConnection::Ptr&)>;
        using IdleCallback = std::function<void(const TcpConnection::Ptr&)>;

        TcpClient(EventLoop* loop, const InetAddress& serverAddr, std::string name);
        TcpClient(const TcpClient&) = delete;
        TcpClient& operator=(const TcpClient&) = delete;
        ~TcpClient();

        void setConnectionCallback(ConnectionCallback cb);
        void setMessageCallback(MessageCallback cb);
        void setFrameMessageCallback(FrameMessageCallback cb);
        void setWriteCompleteCallback(WriteCompleteCallback cb);
        void setHeartbeatCallback(HeartbeatCallback cb);
        void setIdleCallback(IdleCallback cb);
        void setLengthFieldCodec(std::shared_ptr<LengthFieldCodec> codec);
        [[nodiscard]] const std::shared_ptr<LengthFieldCodec>& codec() const noexcept;

        void setHeartbeatInterval(std::chrono::milliseconds interval) noexcept;
        [[nodiscard]] std::chrono::milliseconds heartbeatInterval() const noexcept;

        void setIdleTimeout(std::chrono::milliseconds timeout) noexcept;
        [[nodiscard]] std::chrono::milliseconds idleTimeout() const noexcept;

        void enableRetry(bool on) noexcept;
        [[nodiscard]] bool retryEnabled() const noexcept;

        void setRetryDelayMs(int initialDelayMs, int maxDelayMs) noexcept;

        void connect();
        void disconnect();
        void stop();

        [[nodiscard]] EventLoop* ownerLoop() const noexcept;
        [[nodiscard]] const std::string& name() const noexcept;
        [[nodiscard]] const InetAddress& serverAddress() const noexcept;
        [[nodiscard]] TcpConnection::Ptr connection() const;

    private:
        void newConnection(Socket socket);
        void removeConnection(const TcpConnection::Ptr& conn);
        void startKeepAliveCheck();
        void stopKeepAliveCheck();
        void checkKeepAlive();

    private:
        EventLoop* m_loop{nullptr};
        std::string m_name;
        InetAddress m_serverAddr;
        std::shared_ptr<Connector> m_connector;
        ConnectionCallback m_connectionCallback;
        MessageCallback m_messageCallback;
        FrameMessageCallback m_frameMessageCallback;
        WriteCompleteCallback m_writeCompleteCallback;
        HeartbeatCallback m_heartbeatCallback;
        IdleCallback m_idleCallback;
        std::shared_ptr<LengthFieldCodec> m_codec;
        std::chrono::milliseconds m_heartbeatInterval{0};
        std::chrono::milliseconds m_idleTimeout{0};
        EventLoop::TimerId m_keepAliveTimerId{0};
        bool m_retry{false};
        std::atomic<bool> m_connectRequested{false};
        mutable std::mutex m_mutex;
        TcpConnection::Ptr m_connection;
};
}  // namespace dbase::net