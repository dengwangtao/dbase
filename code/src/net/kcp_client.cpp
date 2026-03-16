#include "dbase/net/kcp_client.h"

#include "dbase/log/log.h"

#include <array>
#include <stdexcept>
#include <utility>
#include <vector>

namespace dbase::net
{
namespace
{
constexpr std::size_t kUdpReadBufferSize = 64 * 1024;
}

KcpClient::KcpClient(
        EventLoop* loop,
        const InetAddress& serverAddr,
        std::string name,
        KcpSession::Options options)
    : m_loop(loop),
      m_name(name.empty() ? "KcpClient" : std::move(name)),
      m_serverAddr(serverAddr),
      m_options(options)
{
    if (m_loop == nullptr)
    {
        throw std::invalid_argument("KcpClient loop is null");
    }
    if (m_options.conv == 0)
    {
        throw std::invalid_argument("KcpClient options.conv must not be 0");
    }
}

KcpClient::~KcpClient()
{
    stop();
}

void KcpClient::setConnectionCallback(ConnectionCallback cb)
{
    m_connectionCallback = std::move(cb);
}

void KcpClient::setMessageCallback(MessageCallback cb)
{
    m_messageCallback = std::move(cb);
}

void KcpClient::setWriteCompleteCallback(WriteCompleteCallback cb)
{
    m_writeCompleteCallback = std::move(cb);
}

void KcpClient::setCloseCallback(CloseCallback cb)
{
    m_closeCallback = std::move(cb);
}

void KcpClient::setErrorCallback(ErrorCallback cb)
{
    m_errorCallback = std::move(cb);
}

void KcpClient::enableEdgeTriggered(bool on) noexcept
{
#if defined(__linux__)
    m_edgeTriggered = on;
#else
    m_edgeTriggered = false;
    (void)on;
#endif
}

void KcpClient::connect()
{
    if (m_started)
    {
        return;
    }

    m_loop->assertInLoopThread();

    m_socket = UdpSocket(Socket::createUdp(m_serverAddr.addressFamily()));
    m_socket.connect(m_serverAddr);

    m_channel = std::make_unique<Channel>(m_loop, m_socket.fd());
#if defined(__linux__)
    m_channel->setEdgeTriggered(m_edgeTriggered);
#else
    m_channel->setEdgeTriggered(false);
#endif
    m_channel->setReadCallback([this]()
                               { handleRead(); });
    m_channel->enableReading();

    auto session = std::make_shared<KcpSession>(m_loop, &m_socket, m_serverAddr, m_options);
    session->setMessageCallback(m_messageCallback);
    session->setWriteCompleteCallback(m_writeCompleteCallback);
    session->setErrorCallback(m_errorCallback);
    session->setCloseCallback(
            [this](const SessionPtr& s)
            {
                handleSessionClose(s);
            });

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_session = session;
    }

    session->connectEstablished();

    if (m_connectionCallback)
    {
        m_connectionCallback(session);
    }

    m_started = true;
}

void KcpClient::disconnect()
{
    SessionPtr s;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        s = m_session;
    }
    if (s)
    {
        s->shutdown();
    }
}

void KcpClient::stop()
{
    if (!m_started)
    {
        return;
    }

    SessionPtr s;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        s = m_session;
        m_session.reset();
    }

    if (s)
    {
        m_loop->runInLoop(
                [s]()
                {
                    s->forceClose();
                    s->connectDestroyed();
                });
    }

    if (m_channel)
    {
        m_loop->runInLoop(
                [this]()
                {
                    if (m_channel)
                    {
                        m_channel->disableAll();
                        m_channel->remove();
                        m_channel.reset();
                    }
                });
    }

    m_started = false;
}

EventLoop* KcpClient::ownerLoop() const noexcept
{
    return m_loop;
}

const std::string& KcpClient::name() const noexcept
{
    return m_name;
}

const InetAddress& KcpClient::serverAddress() const noexcept
{
    return m_serverAddr;
}

KcpClient::SessionPtr KcpClient::session() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_session;
}

void KcpClient::handleRead()
{
    m_loop->assertInLoopThread();

    SessionPtr s;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        s = m_session;
    }
    if (!s)
    {
        return;
    }

    std::array<std::byte, kUdpReadBufferSize> buffer{};

    for (;;)
    {
        auto ret = m_socket.receive(std::span<std::byte>(buffer.data(), buffer.size()));
        if (!ret)
        {
            if (ret.error().code() == dbase::ErrorCode::WouldBlock)
            {
                break;
            }
            throw std::runtime_error("KcpClient receive failed: " + ret.error().message());
        }

        const auto packet = std::span<const std::byte>(buffer.data(), ret.value());
        s->inputPacket(packet);
    }
}

void KcpClient::handleSessionClose(const SessionPtr& session)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_session == session)
        {
            m_session.reset();
        }
    }

    if (m_closeCallback)
    {
        m_closeCallback(session);
    }
}
}  // namespace dbase::net