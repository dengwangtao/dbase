#include "dbase/net/tcp_server.h"

#include <stdexcept>
#include <utility>

namespace dbase::net
{
TcpServer::TcpServer(
        EventLoop* loop,
        const InetAddress& listenAddr,
        std::string name,
        bool reusePort,
        bool ipv6Only)
    : m_loop(loop),
      m_name(name.empty() ? "TcpServer" : std::move(name)),
      m_acceptor(listenAddr, reusePort, ipv6Only)
{
    if (m_loop == nullptr)
    {
        throw std::invalid_argument("TcpServer loop is null");
    }

    m_acceptor.setNewConnectionCallback(
            [this](Socket socket, const InetAddress& peerAddr)
            {
                newConnection(std::move(socket), peerAddr);
            });
}

TcpServer::~TcpServer() = default;

void TcpServer::setConnectionCallback(ConnectionCallback cb)
{
    m_connectionCallback = std::move(cb);
}

void TcpServer::setMessageCallback(MessageCallback cb)
{
    m_messageCallback = std::move(cb);
}

void TcpServer::setFrameMessageCallback(FrameMessageCallback cb)
{
    m_frameMessageCallback = std::move(cb);
}

void TcpServer::setWriteCompleteCallback(WriteCompleteCallback cb)
{
    m_writeCompleteCallback = std::move(cb);
}

void TcpServer::setHighWaterMarkCallback(HighWaterMarkCallback cb)
{
    m_highWaterMarkCallback = std::move(cb);
}

void TcpServer::setLengthFieldCodec(std::shared_ptr<LengthFieldCodec> codec)
{
    if (m_started)
    {
        throw std::logic_error("TcpServer::setLengthFieldCodec after start");
    }

    m_codec = std::move(codec);
}

const std::shared_ptr<LengthFieldCodec>& TcpServer::codec() const noexcept
{
    return m_codec;
}

void TcpServer::setThreadCount(std::size_t threadCount)
{
    if (m_started)
    {
        throw std::logic_error("TcpServer::setThreadCount after start");
    }

    m_threadCount = threadCount;
}

void TcpServer::setThreadInitCallback(ThreadInitCallback cb)
{
    if (m_started)
    {
        throw std::logic_error("TcpServer::setThreadInitCallback after start");
    }

    m_threadInitCallback = std::move(cb);
}

void TcpServer::setMaxOutputBufferBytes(std::size_t bytes) noexcept
{
    m_maxOutputBufferBytes = bytes;
}

void TcpServer::setOutputOverflowPolicy(TcpConnection::OutputOverflowPolicy policy) noexcept
{
    m_outputOverflowPolicy = policy;
}

void TcpServer::start()
{
    if (m_started)
    {
        return;
    }

    m_loop->assertInLoopThread();

    m_threadPool = std::make_unique<EventLoopThreadPool>(m_loop, m_name + "-io", m_threadCount);
    m_threadPool->start();

    if (m_threadInitCallback)
    {
        if (m_threadPool->loops().empty())
        {
            m_threadInitCallback();
        }
        else
        {
            for (auto* loop : m_threadPool->loops())
            {
                loop->runInLoop(m_threadInitCallback);
            }
        }
    }

    m_acceptor.listen();

    m_acceptChannel = std::make_unique<Channel>(m_loop, m_acceptor.socket().fd());
    m_acceptChannel->setReadCallback([this]()
                                     { m_acceptor.acceptAvailable(64); });
    m_acceptChannel->enableReading();

    m_started = true;
}

EventLoop* TcpServer::ownerLoop() const noexcept
{
    return m_loop;
}

const std::string& TcpServer::name() const noexcept
{
    return m_name;
}

bool TcpServer::started() const noexcept
{
    return m_started;
}

std::size_t TcpServer::connectionCount() const noexcept
{
    return m_connections.size();
}

std::size_t TcpServer::threadCount() const noexcept
{
    return m_threadCount;
}

void TcpServer::newConnection(Socket socket, const InetAddress& peerAddr)
{
    m_loop->assertInLoopThread();

    EventLoop* ioLoop = m_loop;
    if (m_threadPool != nullptr)
    {
        ioLoop = m_threadPool->getNextLoop();
    }

    const auto connName = m_name + "-" + std::to_string(m_nextConnId++);
    const auto localAddr = socket.localAddress();

    auto conn = std::make_shared<TcpConnection>(
            ioLoop,
            connName,
            std::move(socket),
            localAddr,
            peerAddr);

    conn->setConnectionCallback(m_connectionCallback);
    conn->setMessageCallback(m_messageCallback);
    conn->setFrameMessageCallback(m_frameMessageCallback);
    conn->setWriteCompleteCallback(m_writeCompleteCallback);
    conn->setHighWaterMarkCallback(m_highWaterMarkCallback);
    conn->setLengthFieldCodec(m_codec);
    conn->setMaxOutputBufferBytes(m_maxOutputBufferBytes);
    conn->setOutputOverflowPolicy(m_outputOverflowPolicy);
    conn->setCloseCallback([this](const TcpConnection::Ptr& c)
                           { removeConnection(c); });

    m_connections.emplace(connName, conn);

    ioLoop->runInLoop([conn]()
                      { conn->connectEstablished(); });
}

void TcpServer::removeConnection(const TcpConnection::Ptr& conn)
{
    auto selfConn = conn;
    m_loop->queueInLoop([this, selfConn]()
                        { removeConnectionInLoop(selfConn); });
}

void TcpServer::removeConnectionInLoop(const TcpConnection::Ptr& conn)
{
    m_loop->assertInLoopThread();

    const auto erased = m_connections.erase(conn->name());
    if (erased == 0)
    {
        return;
    }

    auto* ioLoop = conn->ownerLoop();
    ioLoop->queueInLoop([conn]()
                        { conn->connectDestroyed(); });
}

}  // namespace dbase::net