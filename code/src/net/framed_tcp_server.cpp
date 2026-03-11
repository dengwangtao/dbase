#include "dbase/net/framed_tcp_server.h"

#include <stdexcept>
#include <utility>

namespace dbase::net
{
FramedTcpServer::FramedTcpServer(
        EventLoop* loop,
        const InetAddress& listenAddr,
        std::string name,
        LengthFieldCodec codec,
        bool reusePort,
        bool ipv6Only)
    : m_server(loop, listenAddr, std::move(name), reusePort, ipv6Only),
      m_codec(std::move(codec))
{
    m_server.setConnectionCallback([this](const TcpConnection::Ptr& conn)
                                   {
        if (m_connectionCallback)
        {
            m_connectionCallback(conn);
        } });

    m_server.setWriteCompleteCallback([this](const TcpConnection::Ptr& conn)
                                      {
        if (m_writeCompleteCallback)
        {
            m_writeCompleteCallback(conn);
        } });

    m_server.setMessageCallback([this](const TcpConnection::Ptr& conn, Buffer& buffer)
                                { onRawMessage(conn, buffer); });
}

void FramedTcpServer::setConnectionCallback(ConnectionCallback cb)
{
    m_connectionCallback = std::move(cb);
}

void FramedTcpServer::setWriteCompleteCallback(WriteCompleteCallback cb)
{
    m_writeCompleteCallback = std::move(cb);
}

void FramedTcpServer::setThreadInitCallback(ThreadInitCallback cb)
{
    m_threadInitCallback = std::move(cb);
    m_server.setThreadInitCallback(m_threadInitCallback);
}

void FramedTcpServer::setFrameMessageCallback(FrameMessageCallback cb)
{
    m_frameMessageCallback = std::move(cb);
}

void FramedTcpServer::setThreadCount(std::size_t threadCount)
{
    m_server.setThreadCount(threadCount);
}

void FramedTcpServer::start()
{
    m_server.start();
}

TcpServer& FramedTcpServer::rawServer() noexcept
{
    return m_server;
}

const TcpServer& FramedTcpServer::rawServer() const noexcept
{
    return m_server;
}

const LengthFieldCodec& FramedTcpServer::codec() const noexcept
{
    return m_codec;
}

void FramedTcpServer::send(const TcpConnection::Ptr& conn, std::string_view payload) const
{
    if (!conn)
    {
        throw std::invalid_argument("FramedTcpServer::send conn is null");
    }

    Buffer out;
    m_codec.encode(payload, out);
    conn->send(out);
}

void FramedTcpServer::shutdown(const TcpConnection::Ptr& conn) const
{
    if (!conn)
    {
        return;
    }

    conn->shutdown();
}

void FramedTcpServer::forceClose(const TcpConnection::Ptr& conn) const
{
    if (!conn)
    {
        return;
    }

    conn->forceClose();
}

void FramedTcpServer::onRawMessage(const TcpConnection::Ptr& conn, Buffer& buffer)
{
    for (;;)
    {
        auto result = m_codec.tryDecode(buffer);
        switch (result.status)
        {
            case LengthFieldCodec::DecodeStatus::Ok:
                if (m_frameMessageCallback)
                {
                    m_frameMessageCallback(conn, std::move(result.payload));
                }
                break;

            case LengthFieldCodec::DecodeStatus::NeedMoreData:
                return;

            case LengthFieldCodec::DecodeStatus::InvalidLength:
            case LengthFieldCodec::DecodeStatus::ExceedMaxFrameLength:
                if (conn)
                {
                    conn->forceClose();
                }
                return;

            default:
                if (conn)
                {
                    conn->forceClose();
                }
                return;
        }
    }
}

}  // namespace dbase::net