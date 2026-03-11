#include "dbase/net/connector.h"

#include "dbase/net/socket_ops.h"

#include <chrono>
#include <stdexcept>
#include <utility>

#if defined(_WIN32)
#include <WinSock2.h>
#else
#include <cerrno>
#endif

namespace dbase::net
{
namespace
{
bool isInProgressError(int err) noexcept
{
#if defined(_WIN32)
    return err == WSAEWOULDBLOCK || err == WSAEINPROGRESS || err == WSAEALREADY;
#else
    return err == EINPROGRESS || err == EALREADY || err == EWOULDBLOCK;
#endif
}

bool isRetryableConnectError(int err) noexcept
{
#if defined(_WIN32)
    return err == WSAECONNREFUSED || err == WSAENETUNREACH || err == WSAEHOSTUNREACH || err == WSAETIMEDOUT || err == WSAECONNRESET;
#else
    return err == ECONNREFUSED || err == ENETUNREACH || err == EHOSTUNREACH || err == ETIMEDOUT || err == ECONNRESET;
#endif
}

int lastSocketErrorCode() noexcept
{
#if defined(_WIN32)
    return ::WSAGetLastError();
#else
    return errno;
#endif
}
}  // namespace

Connector::Connector(EventLoop* loop, const InetAddress& serverAddr)
    : m_loop(loop),
      m_serverAddr(serverAddr),
      m_retryTimer(std::make_unique<dbase::thread::TimerQueue>(nullptr, "connector-retry"))
{
    if (m_loop == nullptr)
    {
        throw std::invalid_argument("Connector loop is null");
    }

    m_retryTimer->start();
}

Connector::~Connector()
{
    cancelRetry();

    if (m_retryTimer)
    {
        m_retryTimer->stop();
    }

    if (m_channel)
    {
        m_channel.reset();
    }

    if (m_socket != kInvalidSocket)
    {
        SocketOps::close(m_socket);
        m_socket = kInvalidSocket;
    }

    m_state = State::Disconnected;
    m_started = false;
}

void Connector::setNewConnectionCallback(NewConnectionCallback cb)
{
    m_newConnectionCallback = std::move(cb);
}

void Connector::start()
{
    m_started = true;
    auto self = shared_from_this();
    m_loop->runInLoop([self]()
                      { self->startInLoop(); });
}

void Connector::stop()
{
    m_started = false;
    cancelRetry();

    auto self = shared_from_this();
    m_loop->queueInLoop([self]()
                        { self->stopInLoop(); });
}

void Connector::restart()
{
    cancelRetry();
    m_retryDelayMs = 500;
    m_started = true;

    auto self = shared_from_this();
    m_loop->queueInLoop([self]()
                        {
        self->setState(State::Disconnected);
        self->startInLoop(); });
}

EventLoop* Connector::ownerLoop() const noexcept
{
    return m_loop;
}

const InetAddress& Connector::serverAddress() const noexcept
{
    return m_serverAddr;
}

Connector::State Connector::state() const noexcept
{
    return m_state;
}

bool Connector::started() const noexcept
{
    return m_started;
}

void Connector::startInLoop()
{
    m_loop->assertInLoopThread();

    if (!m_started)
    {
        return;
    }

    if (m_state == State::Disconnected)
    {
        connect();
    }
}

void Connector::stopInLoop()
{
    m_loop->assertInLoopThread();
    m_started = false;

    if (m_state == State::Connecting)
    {
        const SocketType sock = removeAndResetChannelRaw();
        setState(State::Disconnected);
        if (sock != kInvalidSocket)
        {
            SocketOps::close(sock);
        }
    }
}

void Connector::connect()
{
    m_loop->assertInLoopThread();

    SocketType sock = createNonblockingSocket();
    auto ret = SocketOps::connect(sock, m_serverAddr);
    if (ret)
    {
        const int err = SocketOps::getSocketError(sock);
        if (err == 0 || isInProgressError(err))
        {
            connecting(Socket(sock));
            return;
        }

        if (isRetryableConnectError(err))
        {
            SocketOps::close(sock);
            retry();
            return;
        }

        SocketOps::close(sock);
        return;
    }

    const int err = lastSocketErrorCode();
    if (isInProgressError(err))
    {
        connecting(Socket(sock));
        return;
    }

    SocketOps::close(sock);
    if (isRetryableConnectError(err))
    {
        retry();
    }
}

void Connector::connecting(Socket socket)
{
    m_loop->assertInLoopThread();

    setState(State::Connecting);
    m_socket = socket.release();
    m_channel = std::make_unique<Channel>(m_loop, m_socket);

    auto self = shared_from_this();
    m_channel->setWriteCallback([self]()
                                { self->handleWrite(); });
    m_channel->setErrorCallback([self]()
                                { self->handleError(); });
    m_channel->enableWriting();
}

void Connector::handleWrite()
{
    m_loop->assertInLoopThread();

    if (m_state != State::Connecting)
    {
        return;
    }

    const SocketType sock = removeAndResetChannelRaw();
    const int err = SocketOps::getSocketError(sock);

    if (err != 0)
    {
        SocketOps::close(sock);
        retry();
        return;
    }

    Socket socket(sock);
    if (socket.isSelfConnect())
    {
        retry();
        return;
    }

    setState(State::Connected);

    if (m_started && m_newConnectionCallback)
    {
        m_newConnectionCallback(std::move(socket));
    }
    else
    {
        socket.reset();
    }
}

void Connector::handleError()
{
    m_loop->assertInLoopThread();

    if (m_state != State::Connecting)
    {
        return;
    }

    const SocketType sock = removeAndResetChannelRaw();
    if (sock != kInvalidSocket)
    {
        SocketOps::close(sock);
    }

    retry();
}

void Connector::retry()
{
    m_loop->assertInLoopThread();

    setState(State::Disconnected);

    if (!m_started)
    {
        return;
    }

    cancelRetry();

    auto self = shared_from_this();
    const auto delay = std::chrono::milliseconds(m_retryDelayMs);

    m_retryTimerId = m_retryTimer->runAfter(delay, [self]()
                                            { self->m_loop->queueInLoop([self]()
                                                                        { self->startInLoop(); }); });

    if (m_retryDelayMs < 30000)
    {
        m_retryDelayMs *= 2;
        if (m_retryDelayMs > 30000)
        {
            m_retryDelayMs = 30000;
        }
    }
}

void Connector::cancelRetry()
{
    if (m_retryTimer && m_retryTimerId != 0)
    {
        m_retryTimer->cancel(m_retryTimerId);
        m_retryTimerId = 0;
    }
}

void Connector::removeAndResetChannel()
{
    m_loop->assertInLoopThread();
    (void)removeAndResetChannelRaw();
}

SocketType Connector::removeAndResetChannelRaw()
{
    m_loop->assertInLoopThread();

    SocketType sock = m_socket;

    if (m_channel)
    {
        m_channel->disableAll();
        m_channel->remove();

        Channel* rawChannel = m_channel.release();
        m_loop->queueInLoop([rawChannel]()
                            { delete rawChannel; });
    }

    m_socket = kInvalidSocket;
    return sock;
}

SocketType Connector::createNonblockingSocket() const
{
    return SocketOps::createTcpNonblockingOrDie(m_serverAddr.addressFamily());
}

void Connector::setState(State state) noexcept
{
    m_state = state;
}

}  // namespace dbase::net