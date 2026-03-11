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

void TcpServer::setWriteCompleteCallback(WriteCompleteCallback cb)
{
    m_writeCompleteCallback = std::move(cb);
}

void TcpServer::start()
{
    if (m_started)
    {
        return;
    }

    m_loop->assertInLoopThread();

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

void TcpServer::newConnection(Socket socket, const InetAddress& peerAddr)
{
    m_loop->assertInLoopThread();

    const auto connName = m_name + "-" + std::to_string(m_nextConnId++);
    const auto localAddr = socket.localAddress();

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

    m_connections.emplace(connName, conn);
    conn->connectEstablished();
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
    if (erased > 0)
    {
        conn->connectDestroyed();
    }
}

}  // namespace dbase::net