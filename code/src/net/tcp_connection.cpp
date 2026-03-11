#include "dbase/net/tcp_connection.h"

#include "dbase/net/socket_ops.h"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <WinSock2.h>
#else
#include <cerrno>
#endif

namespace dbase::net
{
namespace
{
bool isWouldBlockError(int err) noexcept
{
#if defined(_WIN32)
    return err == WSAEWOULDBLOCK;
#else
    return err == EAGAIN || err == EWOULDBLOCK;
#endif
}

bool isConnectionResetError(int err) noexcept
{
#if defined(_WIN32)
    return err == WSAECONNRESET || err == WSAECONNABORTED;
#else
    return err == ECONNRESET || err == EPIPE;
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

TcpConnection::TcpConnection(
        EventLoop* loop,
        std::string name,
        Socket socket,
        const InetAddress& localAddr,
        const InetAddress& peerAddr)
    : m_loop(loop),
      m_name(name.empty() ? "conn" : std::move(name)),
      m_socket(std::move(socket)),
      m_localAddr(localAddr),
      m_peerAddr(peerAddr),
      m_channel(std::make_unique<Channel>(loop, m_socket.fd()))
{
    if (m_loop == nullptr)
    {
        throw std::invalid_argument("TcpConnection loop is null");
    }

    m_channel->setReadCallback([this]()
                               { handleRead(); });
    m_channel->setWriteCallback([this]()
                                { handleWrite(); });
    m_channel->setCloseCallback([this]()
                                { handleClose(); });
    m_channel->setErrorCallback([this]()
                                { handleError(); });
}

TcpConnection::~TcpConnection() = default;

EventLoop* TcpConnection::ownerLoop() const noexcept
{
    return m_loop;
}

const std::string& TcpConnection::name() const noexcept
{
    return m_name;
}

TcpConnection::State TcpConnection::state() const noexcept
{
    return m_state;
}

bool TcpConnection::connected() const noexcept
{
    return m_state == State::Connected;
}

bool TcpConnection::disconnected() const noexcept
{
    return m_state == State::Disconnected;
}

const InetAddress& TcpConnection::localAddress() const noexcept
{
    return m_localAddr;
}

const InetAddress& TcpConnection::peerAddress() const noexcept
{
    return m_peerAddr;
}

SocketType TcpConnection::fd() const noexcept
{
    return m_socket.fd();
}

Buffer& TcpConnection::inputBuffer() noexcept
{
    return m_inputBuffer;
}

Buffer& TcpConnection::outputBuffer() noexcept
{
    return m_outputBuffer;
}

const Buffer& TcpConnection::inputBuffer() const noexcept
{
    return m_inputBuffer;
}

const Buffer& TcpConnection::outputBuffer() const noexcept
{
    return m_outputBuffer;
}

void TcpConnection::setConnectionCallback(ConnectionCallback cb)
{
    m_connectionCallback = std::move(cb);
}

void TcpConnection::setMessageCallback(MessageCallback cb)
{
    m_messageCallback = std::move(cb);
}

void TcpConnection::setFrameMessageCallback(FrameMessageCallback cb)
{
    m_frameMessageCallback = std::move(cb);
}

void TcpConnection::setWriteCompleteCallback(WriteCompleteCallback cb)
{
    m_writeCompleteCallback = std::move(cb);
}

void TcpConnection::setCloseCallback(CloseCallback cb)
{
    m_closeCallback = std::move(cb);
}

void TcpConnection::setErrorCallback(ErrorCallback cb)
{
    m_errorCallback = std::move(cb);
}

void TcpConnection::setLengthFieldCodec(std::shared_ptr<LengthFieldCodec> codec)
{
    m_codec = std::move(codec);
}

const std::shared_ptr<LengthFieldCodec>& TcpConnection::codec() const noexcept
{
    return m_codec;
}

void TcpConnection::connectEstablished()
{
    m_loop->assertInLoopThread();

    if (m_state != State::Connecting)
    {
        throw std::logic_error("TcpConnection::connectEstablished invalid state");
    }

    setState(State::Connected);
    m_channel->enableReading();

    if (m_connectionCallback)
    {
        m_connectionCallback(shared_from_this());
    }
}

void TcpConnection::connectDestroyed()
{
    m_loop->assertInLoopThread();

    if (m_state != State::Disconnected)
    {
        setState(State::Disconnected);

        if (m_connectionCallback)
        {
            m_connectionCallback(shared_from_this());
        }
    }

    if (m_channel)
    {
        m_channel->disableAll();
        m_channel->remove();
    }
}

void TcpConnection::send(std::string_view data)
{
    if (m_state == State::Disconnected)
    {
        return;
    }

    if (data.empty())
    {
        return;
    }

    if (m_loop->isInLoopThread())
    {
        sendInLoop(std::string(data));
        return;
    }

    auto self = shared_from_this();
    std::string copied(data);
    m_loop->queueInLoop([self, copied = std::move(copied)]() mutable
                        { self->sendInLoop(std::move(copied)); });
}

void TcpConnection::send(Buffer& buffer)
{
    send(buffer.readableView());
    buffer.retrieveAll();
}

void TcpConnection::sendFrame(std::string_view payload)
{
    if (!m_codec)
    {
        send(payload);
        return;
    }

    Buffer frame;
    m_codec->encode(payload, frame);
    send(frame);
}

void TcpConnection::shutdown()
{
    if (m_state != State::Connected)
    {
        return;
    }

    setState(State::Disconnecting);

    auto self = shared_from_this();
    if (m_loop->isInLoopThread())
    {
        shutdownInLoop();
        return;
    }

    m_loop->queueInLoop([self]()
                        { self->shutdownInLoop(); });
}

void TcpConnection::forceClose()
{
    if (m_state == State::Disconnected)
    {
        return;
    }

    auto self = shared_from_this();
    if (m_loop->isInLoopThread())
    {
        forceCloseInLoop();
        return;
    }

    m_loop->queueInLoop([self]()
                        { self->forceCloseInLoop(); });
}

void TcpConnection::sendInLoop(std::string data)
{
    m_loop->assertInLoopThread();

    if (m_state == State::Disconnected)
    {
        return;
    }

    if (data.empty())
    {
        return;
    }

    if (!m_channel->isWriting() && m_outputBuffer.readableBytes() == 0)
    {
        const int n = SocketOps::write(m_socket.fd(), data.data(), data.size());
        if (n >= 0)
        {
            const auto written = static_cast<std::size_t>(n);
            if (written < data.size())
            {
                m_outputBuffer.append(data.data() + written, data.size() - written);
                m_channel->enableWriting();
            }
            else if (m_writeCompleteCallback)
            {
                m_writeCompleteCallback(shared_from_this());
            }
            return;
        }

        const int err = lastSocketErrorCode();
        if (isWouldBlockError(err))
        {
            m_outputBuffer.append(data);
            m_channel->enableWriting();
            return;
        }

        handleError();
        return;
    }

    m_outputBuffer.append(data);
    if (!m_channel->isWriting())
    {
        m_channel->enableWriting();
    }
}

void TcpConnection::shutdownInLoop()
{
    m_loop->assertInLoopThread();

    if (!m_channel->isWriting() && m_outputBuffer.readableBytes() == 0)
    {
        m_socket.shutdownWrite();
    }
}

void TcpConnection::forceCloseInLoop()
{
    m_loop->assertInLoopThread();

    if (m_state == State::Disconnected)
    {
        return;
    }

    handleClose();
}

void TcpConnection::handleRead()
{
    m_loop->assertInLoopThread();

    std::vector<char> buf(64 * 1024);
    const int n = SocketOps::read(m_socket.fd(), buf.data(), buf.size());

    if (n > 0)
    {
        m_inputBuffer.append(buf.data(), static_cast<std::size_t>(n));

        if (m_codec && m_frameMessageCallback)
        {
            handleCodecMessages();
        }
        else if (m_messageCallback)
        {
            m_messageCallback(shared_from_this(), m_inputBuffer);
        }

        return;
    }

    if (n == 0)
    {
        handleClose();
        return;
    }

    const int err = lastSocketErrorCode();
    if (isWouldBlockError(err))
    {
        return;
    }

    if (isConnectionResetError(err))
    {
        handleClose();
        return;
    }

    handleError();
}

void TcpConnection::handleWrite()
{
    m_loop->assertInLoopThread();

    if (!m_channel->isWriting())
    {
        return;
    }

    if (m_outputBuffer.readableBytes() == 0)
    {
        m_channel->disableWriting();

        if (m_writeCompleteCallback)
        {
            m_writeCompleteCallback(shared_from_this());
        }

        if (m_state == State::Disconnecting)
        {
            shutdownInLoop();
        }

        return;
    }

    const int n = SocketOps::write(m_socket.fd(), m_outputBuffer.peek(), m_outputBuffer.readableBytes());
    if (n > 0)
    {
        m_outputBuffer.retrieve(static_cast<std::size_t>(n));

        if (m_outputBuffer.readableBytes() == 0)
        {
            m_channel->disableWriting();

            if (m_writeCompleteCallback)
            {
                m_writeCompleteCallback(shared_from_this());
            }

            if (m_state == State::Disconnecting)
            {
                shutdownInLoop();
            }
        }

        return;
    }

    const int err = lastSocketErrorCode();
    if (isWouldBlockError(err))
    {
        return;
    }

    handleError();
}

void TcpConnection::handleClose()
{
    m_loop->assertInLoopThread();

    if (m_state == State::Disconnected)
    {
        return;
    }

    setState(State::Disconnected);
    m_channel->disableAll();

    auto self = shared_from_this();

    if (m_connectionCallback)
    {
        m_connectionCallback(self);
    }

    if (m_closeCallback)
    {
        m_closeCallback(self);
    }
}

void TcpConnection::handleError()
{
    m_loop->assertInLoopThread();

    const int err = SocketOps::getSocketError(m_socket.fd());
    if (m_errorCallback)
    {
        m_errorCallback(shared_from_this(), err);
    }
}

void TcpConnection::handleCodecMessages()
{
    for (;;)
    {
        auto result = m_codec->tryDecode(m_inputBuffer);

        if (result.status == LengthFieldCodec::DecodeStatus::NeedMoreData)
        {
            break;
        }

        if (result.status != LengthFieldCodec::DecodeStatus::Ok)
        {
            if (m_errorCallback)
            {
                m_errorCallback(shared_from_this(), kCodecError);
            }

            handleClose();
            break;
        }

        m_frameMessageCallback(shared_from_this(), std::move(result.payload));
    }
}

void TcpConnection::setState(State state) noexcept
{
    m_state = state;
}

}  // namespace dbase::net