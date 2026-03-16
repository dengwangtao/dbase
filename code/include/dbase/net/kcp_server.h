#pragma once

#include "dbase/net/channel.h"
#include "dbase/net/event_loop.h"
#include "dbase/net/event_loop_thread_pool.h"
#include "dbase/net/inet_address.h"
#include "dbase/net/kcp_session.h"
#include "dbase/net/udp_socket.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace dbase::net
{
class KcpServer
{
    public:
        using SessionPtr = KcpSession::Ptr;
        using ConnectionCallback = std::function<void(const SessionPtr&)>;
        using MessageCallback = KcpSession::MessageCallback;
        using WriteCompleteCallback = KcpSession::WriteCompleteCallback;
        using CloseCallback = KcpSession::CloseCallback;
        using ErrorCallback = KcpSession::ErrorCallback;
        using ThreadInitCallback = EventLoop::Functor;

        struct SessionOptions
        {
                int mtu{1400};
                int sndWnd{128};
                int rcvWnd{128};
                int nodelay{1};
                int interval{20};
                int resend{2};
                int nc{1};
                std::chrono::milliseconds idleTimeout{0};
                std::size_t maxMessageBytes{16 * 1024 * 1024};
        };

        KcpServer(
                EventLoop* loop,
                const InetAddress& listenAddr,
                std::string name,
                bool reusePort = false,
                bool ipv6Only = false);

        KcpServer(const KcpServer&) = delete;
        KcpServer& operator=(const KcpServer&) = delete;
        ~KcpServer();

        void setConnectionCallback(ConnectionCallback cb);
        void setMessageCallback(MessageCallback cb);
        void setWriteCompleteCallback(WriteCompleteCallback cb);
        void setCloseCallback(CloseCallback cb);
        void setErrorCallback(ErrorCallback cb);
        void setThreadInitCallback(ThreadInitCallback cb);
        void setThreadCount(std::size_t threadCount);
        void setSessionOptions(SessionOptions options) noexcept;
        void enableEdgeTriggered(bool on) noexcept;

        void start();

        [[nodiscard]] EventLoop* ownerLoop() const noexcept;
        [[nodiscard]] const std::string& name() const noexcept;
        [[nodiscard]] const InetAddress& listenAddress() const noexcept;
        [[nodiscard]] bool started() const noexcept;
        [[nodiscard]] std::size_t threadCount() const noexcept;
        [[nodiscard]] std::size_t connectionCount() const noexcept;

    private:
        struct SessionKey
        {
                std::uint32_t conv{0};
                std::uint32_t token{0};
                std::string peer;

                [[nodiscard]] bool operator==(const SessionKey& other) const noexcept
                {
                    return conv == other.conv && token == other.token && peer == other.peer;
                }
        };

        struct SessionKeyHash
        {
                [[nodiscard]] std::size_t operator()(const SessionKey& key) const noexcept
                {
                    std::size_t h1 = std::hash<std::uint32_t>{}(key.conv);
                    std::size_t h2 = std::hash<std::uint32_t>{}(key.token);
                    std::size_t h3 = std::hash<std::string>{}(key.peer);
                    return h1 ^ (h2 << 1) ^ (h3 << 2);
                }
        };

        struct SessionEntry
        {
                SessionPtr session;
                EventLoop* loop{nullptr};
        };

    private:
        void handleRead();
        void handleSessionClose(const SessionPtr& session);
        [[nodiscard]] SessionKey makeSessionKey(std::uint32_t conv, std::uint32_t token, const InetAddress& peer) const;
        [[nodiscard]] KcpSession::Options buildSessionOptions(std::uint32_t conv, std::uint32_t token) const;
        [[nodiscard]] SessionEntry* findSession(const SessionKey& key);
        SessionPtr createSession(std::uint32_t conv, std::uint32_t token, const InetAddress& peer);

    private:
        EventLoop* m_loop{nullptr};
        std::string m_name;
        InetAddress m_listenAddr;
        UdpSocket m_socket;
        std::unique_ptr<Channel> m_channel;
        std::unique_ptr<EventLoopThreadPool> m_threadPool;
        bool m_started{false};
        bool m_edgeTriggered{false};
        std::size_t m_threadCount{0};
        SessionOptions m_sessionOptions;
        ThreadInitCallback m_threadInitCallback;
        ConnectionCallback m_connectionCallback;
        MessageCallback m_messageCallback;
        WriteCompleteCallback m_writeCompleteCallback;
        CloseCallback m_closeCallback;
        ErrorCallback m_errorCallback;
        std::unordered_map<SessionKey, SessionEntry, SessionKeyHash> m_sessions;
};
}  // namespace dbase::net