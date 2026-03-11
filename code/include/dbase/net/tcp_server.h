#pragma once

#include "dbase/net/acceptor.h"
#include "dbase/net/event_loop.h"
#include "dbase/net/event_loop_thread_pool.h"
#include "dbase/net/length_field_codec.h"
#include "dbase/net/tcp_connection.h"

#include <cstdint>
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
        using FrameMessageCallback = TcpConnection::FrameMessageCallback;
        using WriteCompleteCallback = TcpConnection::WriteCompleteCallback;
        using ThreadInitCallback = EventLoop::Functor;

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
        void setFrameMessageCallback(FrameMessageCallback cb);
        void setWriteCompleteCallback(WriteCompleteCallback cb);

        void setLengthFieldCodec(std::shared_ptr<LengthFieldCodec> codec);
        [[nodiscard]] const std::shared_ptr<LengthFieldCodec>& codec() const noexcept;

        void setThreadCount(std::size_t threadCount);
        void setThreadInitCallback(ThreadInitCallback cb);

        void start();

        [[nodiscard]] EventLoop* ownerLoop() const noexcept;
        [[nodiscard]] const std::string& name() const noexcept;
        [[nodiscard]] bool started() const noexcept;
        [[nodiscard]] std::size_t connectionCount() const noexcept;
        [[nodiscard]] std::size_t threadCount() const noexcept;

    private:
        void newConnection(Socket socket, const InetAddress& peerAddr);
        void removeConnection(const TcpConnection::Ptr& conn);
        void removeConnectionInLoop(const TcpConnection::Ptr& conn);

    private:
        EventLoop* m_loop{nullptr};
        std::string m_name;
        Acceptor m_acceptor;
        std::unique_ptr<Channel> m_acceptChannel;
        std::unique_ptr<EventLoopThreadPool> m_threadPool;
        bool m_started{false};
        std::int64_t m_nextConnId{1};
        std::size_t m_threadCount{0};

        std::shared_ptr<LengthFieldCodec> m_codec;

        ThreadInitCallback m_threadInitCallback;
        ConnectionCallback m_connectionCallback;
        MessageCallback m_messageCallback;
        FrameMessageCallback m_frameMessageCallback;
        WriteCompleteCallback m_writeCompleteCallback;

        std::unordered_map<std::string, TcpConnection::Ptr> m_connections;
};
}  // namespace dbase::net