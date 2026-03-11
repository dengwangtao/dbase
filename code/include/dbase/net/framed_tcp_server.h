#pragma once

#include "dbase/net/event_loop.h"
#include "dbase/net/inet_address.h"
#include "dbase/net/length_field_codec.h"
#include "dbase/net/tcp_connection.h"
#include "dbase/net/tcp_server.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace dbase::net
{
class FramedTcpServer
{
    public:
        using ConnectionCallback = TcpConnection::ConnectionCallback;
        using WriteCompleteCallback = TcpConnection::WriteCompleteCallback;
        using ThreadInitCallback = EventLoop::Functor;
        using FrameMessageCallback = std::function<void(const TcpConnection::Ptr&, std::string&&)>;

        FramedTcpServer(
                EventLoop* loop,
                const InetAddress& listenAddr,
                std::string name,
                LengthFieldCodec codec,
                bool reusePort = false,
                bool ipv6Only = false);

        FramedTcpServer(const FramedTcpServer&) = delete;
        FramedTcpServer& operator=(const FramedTcpServer&) = delete;

        ~FramedTcpServer() = default;

        void setConnectionCallback(ConnectionCallback cb);
        void setWriteCompleteCallback(WriteCompleteCallback cb);
        void setThreadInitCallback(ThreadInitCallback cb);
        void setFrameMessageCallback(FrameMessageCallback cb);

        void setThreadCount(std::size_t threadCount);

        void start();

        [[nodiscard]] TcpServer& rawServer() noexcept;
        [[nodiscard]] const TcpServer& rawServer() const noexcept;
        [[nodiscard]] const LengthFieldCodec& codec() const noexcept;

        void send(const TcpConnection::Ptr& conn, std::string_view payload) const;
        void shutdown(const TcpConnection::Ptr& conn) const;
        void forceClose(const TcpConnection::Ptr& conn) const;

    private:
        void onRawMessage(const TcpConnection::Ptr& conn, Buffer& buffer);

    private:
        TcpServer m_server;
        LengthFieldCodec m_codec;
        ConnectionCallback m_connectionCallback;
        WriteCompleteCallback m_writeCompleteCallback;
        ThreadInitCallback m_threadInitCallback;
        FrameMessageCallback m_frameMessageCallback;
};
}  // namespace dbase::net