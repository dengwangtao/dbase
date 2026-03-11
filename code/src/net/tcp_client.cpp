#include "dbase/net/tcp_client.h"

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
    TcpConnection::Ptr conn;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        conn = m_connection;
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

void TcpClient::setWriteCompleteCallback(WriteCompleteCallback cb)
{
    m_writeCompleteCallback = std::move(cb);
}

void TcpClient::enableRetry(bool on) noexcept
{
    m_retry = on;
}

void TcpClient::connect()
{
    m_connector->start();
}

void TcpClient::disconnect()
{
    TcpConnection::Ptr conn;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        conn = m_connection;
    }

    if (conn)
    {
        conn->shutdown();
    }
}

void TcpClient::stop()
{
    m_connector->stop();
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

bool TcpClient::retryEnabled() const noexcept
{
    return m_retry;
}

TcpConnection::Ptr TcpClient::connection() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_connection;
}

void TcpClient::newConnection(Socket socket)
{
    m_loop->assertInLoopThread();

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
    conn->setWriteCompleteCallback(m_writeCompleteCallback);
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
        m_connection.reset();
    }

    m_loop->queueInLoop([conn]()
                        { conn->connectDestroyed(); });

    if (m_retry)
    {
        m_connector->restart();
    }
}

}  // namespace dbase::net