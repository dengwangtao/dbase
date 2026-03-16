#include "dbase/net/tcp_client.h"
#include <algorithm>
#include <stdexcept>
#include <utility>

namespace dbase::net
{
TcpClient::TcpClient(EventLoop* loop, const InetAddress& serverAddr, std::string name)
    : m_loop(loop),
      m_name(name.empty() ? "TcpClient" : std::move(name)),
      m_serverAddr(serverAddr),
      m_connector(std::make_shared<Connector>(loop, serverAddr))
{
    if (m_loop == nullptr)
    {
        throw std::invalid_argument("TcpClient loop is null");
    }

    m_connector->setNewConnectionCallback([this](Socket socket)
                                          { newConnection(std::move(socket)); });
}

TcpClient::~TcpClient()
{
    stopKeepAliveCheck();
    m_connectRequested.store(false, std::memory_order_release);

    TcpConnection::Ptr conn;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        conn = m_connection;
        m_connection.reset();
    }

    if (conn)
    {
        auto* ioLoop = conn->ownerLoop();
        ioLoop->runInLoop([conn]()
                          { conn->connectDestroyed(); });
    }

    m_connector->stop();
}

void TcpClient::setConnectionCallback(ConnectionCallback cb)
{
    m_connectionCallback = std::move(cb);
}

void TcpClient::setMessageCallback(MessageCallback cb)
{
    m_messageCallback = std::move(cb);
}

void TcpClient::setFrameMessageCallback(FrameMessageCallback cb)
{
    m_frameMessageCallback = std::move(cb);
}

void TcpClient::setWriteCompleteCallback(WriteCompleteCallback cb)
{
    m_writeCompleteCallback = std::move(cb);
}

void TcpClient::setHeartbeatCallback(HeartbeatCallback cb)
{
    m_heartbeatCallback = std::move(cb);
}

void TcpClient::setIdleCallback(IdleCallback cb)
{
    m_idleCallback = std::move(cb);
}

void TcpClient::setLengthFieldCodec(std::shared_ptr<LengthFieldCodec> codec)
{
    m_codec = std::move(codec);
}

const std::shared_ptr<LengthFieldCodec>& TcpClient::codec() const noexcept
{
    return m_codec;
}

void TcpClient::setHeartbeatInterval(std::chrono::milliseconds interval) noexcept
{
    m_heartbeatInterval = interval;
}

std::chrono::milliseconds TcpClient::heartbeatInterval() const noexcept
{
    return m_heartbeatInterval;
}

void TcpClient::setIdleTimeout(std::chrono::milliseconds timeout) noexcept
{
    m_idleTimeout = timeout;
}

std::chrono::milliseconds TcpClient::idleTimeout() const noexcept
{
    return m_idleTimeout;
}

void TcpClient::enableRetry(bool on) noexcept
{
    m_retry = on;
}

bool TcpClient::retryEnabled() const noexcept
{
    return m_retry;
}

void TcpClient::setRetryDelayMs(int initialDelayMs, int maxDelayMs) noexcept
{
    m_connector->setRetryDelayMs(initialDelayMs, maxDelayMs);
}

void TcpClient::connect()
{
    m_connectRequested.store(true, std::memory_order_release);
    startKeepAliveCheck();
    m_connector->start();
}

void TcpClient::disconnect()
{
    m_connectRequested.store(false, std::memory_order_release);

    TcpConnection::Ptr conn;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        conn = m_connection;
    }

    if (conn)
    {
        conn->shutdown();
    }
    else
    {
        m_connector->stop();
    }
}

void TcpClient::stop()
{
    m_connectRequested.store(false, std::memory_order_release);
    stopKeepAliveCheck();
    m_connector->stop();

    TcpConnection::Ptr conn;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        conn = m_connection;
    }

    if (conn)
    {
        conn->forceClose();
    }
}

EventLoop* TcpClient::ownerLoop() const noexcept
{
    return m_loop;
}

const std::string& TcpClient::name() const noexcept
{
    return m_name;
}

const InetAddress& TcpClient::serverAddress() const noexcept
{
    return m_serverAddr;
}

TcpConnection::Ptr TcpClient::connection() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_connection;
}

void TcpClient::newConnection(Socket socket)
{
    m_loop->assertInLoopThread();

    if (!m_connectRequested.load(std::memory_order_acquire))
    {
        return;
    }

    const auto peerAddr = socket.peerAddress();
    const auto localAddr = socket.localAddress();
    const auto connName = m_name + "-conn";
    auto conn = std::make_shared<TcpConnection>(
            m_loop,
            connName,
            std::move(socket),
            localAddr,
            peerAddr);

    conn->setConnectionCallback(m_connectionCallback);
    conn->setMessageCallback(m_messageCallback);
    conn->setFrameMessageCallback(m_frameMessageCallback);
    conn->setWriteCompleteCallback(m_writeCompleteCallback);
    conn->setLengthFieldCodec(m_codec);
    conn->setCloseCallback([this](const TcpConnection::Ptr& c)
                           { removeConnection(c); });

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_connection = conn;
    }

    conn->connectEstablished();
}

void TcpClient::removeConnection(const TcpConnection::Ptr& conn)
{
    m_loop->assertInLoopThread();

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_connection == conn)
        {
            m_connection.reset();
        }
    }

    m_loop->queueInLoop([conn]()
                        { conn->connectDestroyed(); });

    if (m_retry && m_connectRequested.load(std::memory_order_acquire))
    {
        m_connector->restart();
    }
}

void TcpClient::startKeepAliveCheck()
{
    if (m_keepAliveTimerId != 0)
    {
        return;
    }

    const auto baseInterval = [&]() -> std::chrono::milliseconds
    {
        if (m_idleTimeout.count() > 0 && m_heartbeatInterval.count() > 0)
        {
            return std::min(std::max(std::chrono::milliseconds(1000), m_idleTimeout / 2), m_heartbeatInterval);
        }
        if (m_idleTimeout.count() > 0)
        {
            return std::max(std::chrono::milliseconds(1000), m_idleTimeout / 2);
        }
        if (m_heartbeatInterval.count() > 0)
        {
            return m_heartbeatInterval;
        }
        return std::chrono::milliseconds(0);
    }();

    if (baseInterval.count() <= 0)
    {
        return;
    }

    m_keepAliveTimerId = m_loop->runEvery(baseInterval, [this]()
                                          { checkKeepAlive(); });
}

void TcpClient::stopKeepAliveCheck()
{
    if (m_keepAliveTimerId != 0)
    {
        m_loop->cancelTimer(m_keepAliveTimerId);
        m_keepAliveTimerId = 0;
    }
}

void TcpClient::checkKeepAlive()
{
    TcpConnection::Ptr conn;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        conn = m_connection;
    }

    if (!conn || !conn->connected())
    {
        return;
    }

    const auto idle = conn->idleFor();
    if (m_idleTimeout.count() > 0 && idle >= m_idleTimeout)
    {
        if (m_idleCallback)
        {
            m_idleCallback(conn);
        }
        else
        {
            conn->forceClose();
        }
        return;
    }

    if (m_heartbeatInterval.count() > 0 && m_heartbeatCallback)
    {
        const auto lastProbe = conn->lastProbeAt();
        const bool neverProbed = (lastProbe == TcpConnection::TimePoint{});
        const bool probeExpired = !neverProbed && conn->probeIdleFor() >= m_heartbeatInterval;
        if (idle >= m_heartbeatInterval && (neverProbed || probeExpired))
        {
            conn->markKeepAliveProbeSent();
            m_heartbeatCallback(conn);
        }
    }
}
}  // namespace dbase::net