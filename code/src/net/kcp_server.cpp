#include "dbase/net/kcp_server.h"

#include "dbase/log/log.h"
#include "dbase/net/kcp_packet.h"

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

KcpServer::KcpServer(
        EventLoop* loop,
        const InetAddress& listenAddr,
        std::string name,
        bool reusePort,
        bool ipv6Only)
    : m_loop(loop),
      m_name(name.empty() ? "KcpServer" : std::move(name)),
      m_listenAddr(listenAddr)
{
    if (m_loop == nullptr)
    {
        throw std::invalid_argument("KcpServer loop is null");
    }

    m_socket = UdpSocket(Socket::createUdp(listenAddr.addressFamily()));
    m_socket.setReuseAddr(true);
    if (reusePort)
    {
        m_socket.setReusePort(true);
    }
    if (listenAddr.addressFamily() == AF_INET6)
    {
        m_socket.socket().setIpv6Only(ipv6Only);
    }
    m_socket.bindAddress(m_listenAddr);
}

KcpServer::~KcpServer() = default;

void KcpServer::setConnectionCallback(ConnectionCallback cb)
{
    m_connectionCallback = std::move(cb);
}

void KcpServer::setMessageCallback(MessageCallback cb)
{
    m_messageCallback = std::move(cb);
}

void KcpServer::setWriteCompleteCallback(WriteCompleteCallback cb)
{
    m_writeCompleteCallback = std::move(cb);
}

void KcpServer::setCloseCallback(CloseCallback cb)
{
    m_closeCallback = std::move(cb);
}

void KcpServer::setErrorCallback(ErrorCallback cb)
{
    m_errorCallback = std::move(cb);
}

void KcpServer::setThreadInitCallback(ThreadInitCallback cb)
{
    if (m_started)
    {
        throw std::logic_error("KcpServer::setThreadInitCallback after start");
    }
    m_threadInitCallback = std::move(cb);
}

void KcpServer::setThreadCount(std::size_t threadCount)
{
    if (m_started)
    {
        throw std::logic_error("KcpServer::setThreadCount after start");
    }
    m_threadCount = threadCount;
}

void KcpServer::setSessionOptions(SessionOptions options) noexcept
{
    m_sessionOptions = options;
}

void KcpServer::enableEdgeTriggered(bool on) noexcept
{
#if defined(__linux__)
    m_edgeTriggered = on;
#else
    m_edgeTriggered = false;
    (void)on;
#endif
}

void KcpServer::start()
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

    m_channel = std::make_unique<Channel>(m_loop, m_socket.fd());
#if defined(__linux__)
    m_channel->setEdgeTriggered(m_edgeTriggered);
#else
    m_channel->setEdgeTriggered(false);
#endif
    m_channel->setReadCallback([this]()
                               { handleRead(); });
    m_channel->enableReading();

    m_started = true;
}

EventLoop* KcpServer::ownerLoop() const noexcept
{
    return m_loop;
}

const std::string& KcpServer::name() const noexcept
{
    return m_name;
}

const InetAddress& KcpServer::listenAddress() const noexcept
{
    return m_listenAddr;
}

bool KcpServer::started() const noexcept
{
    return m_started;
}

std::size_t KcpServer::threadCount() const noexcept
{
    return m_threadCount;
}

std::size_t KcpServer::connectionCount() const noexcept
{
    return m_sessions.size();
}

void KcpServer::handleRead()
{
    m_loop->assertInLoopThread();

    std::array<std::byte, kUdpReadBufferSize> buffer{};

    for (;;)
    {
        auto datagramRet = m_socket.receiveFrom(std::span<std::byte>(buffer.data(), buffer.size()));
        if (!datagramRet)
        {
            if (datagramRet.error().code() == dbase::ErrorCode::WouldBlock)
            {
                break;
            }
            throw std::runtime_error("KcpServer receiveFrom failed: " + datagramRet.error().message());
        }

        const auto& datagram = datagramRet.value();
        const auto packet = std::span<const std::byte>(buffer.data(), datagram.size);

        KcpPacketHeader header;
        if (!KcpPacket::tryParseHeader(packet, header))
        {
            DBASE_LOG_WARN("KcpServer drop short packet from {}", datagram.peer.toIpPort());
            continue;
        }

        const auto key = makeSessionKey(header.conv, header.token, datagram.peer);
        auto* entry = findSession(key);
        if (entry == nullptr)
        {
            auto session = createSession(header.conv, header.token, datagram.peer);
            entry = findSession(key);
            if (entry == nullptr || !session)
            {
                continue;
            }
        }

        auto session = entry->session;
        auto* ioLoop = entry->loop;
        std::vector<std::byte> payload(packet.begin(), packet.end());

        ioLoop->runInLoop(
                [session, packetData = std::move(payload)]() mutable
                {
                    session->inputPacket(std::span<const std::byte>(packetData.data(), packetData.size()));
                });
    }
}

void KcpServer::handleSessionClose(const SessionPtr& session)
{
    m_loop->runInLoop(
            [this, session]()
            {
                for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it)
                {
                    if (it->second.session == session)
                    {
                        m_sessions.erase(it);
                        break;
                    }
                }
                if (m_closeCallback)
                {
                    m_closeCallback(session);
                }
            });
}

KcpServer::SessionKey KcpServer::makeSessionKey(std::uint32_t conv, std::uint32_t token, const InetAddress& peer) const
{
    SessionKey key;
    key.conv = conv;
    key.token = token;
    key.peer = peer.toIpPort();
    return key;
}

KcpSession::Options KcpServer::buildSessionOptions(std::uint32_t conv, std::uint32_t token) const
{
    KcpSession::Options options;
    options.conv = conv;
    options.token = token;
    options.mtu = m_sessionOptions.mtu;
    options.sndWnd = m_sessionOptions.sndWnd;
    options.rcvWnd = m_sessionOptions.rcvWnd;
    options.nodelay = m_sessionOptions.nodelay;
    options.interval = m_sessionOptions.interval;
    options.resend = m_sessionOptions.resend;
    options.nc = m_sessionOptions.nc;
    options.idleTimeout = m_sessionOptions.idleTimeout;
    options.maxMessageBytes = m_sessionOptions.maxMessageBytes;
    return options;
}

KcpServer::SessionEntry* KcpServer::findSession(const SessionKey& key)
{
    const auto it = m_sessions.find(key);
    if (it == m_sessions.end())
    {
        return nullptr;
    }
    return &it->second;
}

KcpServer::SessionPtr KcpServer::createSession(std::uint32_t conv, std::uint32_t token, const InetAddress& peer)
{
    m_loop->assertInLoopThread();

    EventLoop* ioLoop = m_loop;
    if (m_threadPool != nullptr)
    {
        ioLoop = m_threadPool->getLoopForHash(
                std::hash<std::string>{}(peer.toIpPort()) ^ std::hash<std::uint32_t>{}(conv) ^ std::hash<std::uint32_t>{}(token));
    }

    auto options = buildSessionOptions(conv, token);
    auto session = std::make_shared<KcpSession>(ioLoop, &m_socket, peer, options);

    session->setMessageCallback(m_messageCallback);
    session->setWriteCompleteCallback(m_writeCompleteCallback);
    session->setErrorCallback(m_errorCallback);
    session->setCloseCallback(
            [this](const SessionPtr& s)
            {
                handleSessionClose(s);
            });

    const auto key = makeSessionKey(conv, token, peer);
    m_sessions.emplace(key, SessionEntry{session, ioLoop});

    ioLoop->runInLoop(
            [session]()
            {
                session->connectEstablished();
            });

    if (m_connectionCallback)
    {
        m_connectionCallback(session);
    }

    return session;
}
}  // namespace dbase::net