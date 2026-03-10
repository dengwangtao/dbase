#include "dbase/net/tcp_connection.h"

#include "dbase/net/socket_ops.h"

#include <stdexcept>
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
        std::string name,
        Socket socket,
        const InetAddress& localAddr,
        const InetAddress& peerAddr)
    : m_name(name.empty() ? "conn" : std::move(name)),
      m_socket(std::move(socket)),
      m_localAddr(localAddr),
      m_peerAddr(peerAddr)
{
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

void TcpConnection::setMessageCallback(MessageCallback cb)
{
    m_messageCallback = std::move(cb);
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

void TcpConnection::connectEstablished()
{
    if (m_state != State::Connecting)
    {
        throw std::logic_error("TcpConnection::connectEstablished invalid state");
    }

    m_state = State::Connected;
}

void TcpConnection::connectDestroyed()
{
    if (m_state == State::Connected || m_state == State::Disconnecting)
    {
        m_state = State::Disconnected;
    }
}

void TcpConnection::send(std::string_view data)
{
    if (!connected() && m_state != State::Disconnecting)
    {
        throw std::logic_error("TcpConnection::send on disconnected connection");
    }

    if (data.empty())
    {
        return;
    }

    if (m_outputBuffer.readableBytes() == 0)
    {
        const int n = SocketOps::write(m_socket.fd(), data.data(), data.size());
        if (n >= 0)
        {
            const auto written = static_cast<std::size_t>(n);
            if (written < data.size())
            {
                m_outputBuffer.append(data.data() + written, data.size() - written);
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
            return;
        }

        handleError(err);
        return;
    }

    m_outputBuffer.append(data);
}

void TcpConnection::send(Buffer& buffer)
{
    send(buffer.readableView());
    buffer.retrieveAll();
}

void TcpConnection::shutdown()
{
    if (m_state == State::Connected)
    {
        m_state = State::Disconnecting;
        if (m_outputBuffer.readableBytes() == 0)
        {
            m_socket.shutdownWrite();
        }
    }
}

void TcpConnection::forceClose()
{
    if (m_state != State::Disconnected)
    {
        handleClose();
    }
}

std::size_t TcpConnection::receiveOnce(std::size_t maxBytes)
{
    if (!connected() && m_state != State::Disconnecting)
    {
        return 0;
    }

    if (maxBytes == 0)
    {
        return 0;
    }

    std::vector<char> buf(maxBytes);
    const int n = SocketOps::read(m_socket.fd(), buf.data(), buf.size());

    if (n > 0)
    {
        const auto readBytes = static_cast<std::size_t>(n);
        m_inputBuffer.append(buf.data(), readBytes);

        if (m_messageCallback)
        {
            m_messageCallback(shared_from_this(), m_inputBuffer);
        }

        return readBytes;
    }

    if (n == 0)
    {
        handleClose();
        return 0;
    }

    const int err = lastSocketErrorCode();
    if (isWouldBlockError(err))
    {
        return 0;
    }

    if (isConnectionResetError(err))
    {
        handleClose();
        return 0;
    }

    handleError(err);
    return 0;
}

std::size_t TcpConnection::flushOutput()
{
    if (m_outputBuffer.readableBytes() == 0)
    {
        if (m_state == State::Disconnecting)
        {
            m_socket.shutdownWrite();
        }
        return 0;
    }

    const int n = SocketOps::write(m_socket.fd(), m_outputBuffer.peek(), m_outputBuffer.readableBytes());
    if (n > 0)
    {
        const auto written = static_cast<std::size_t>(n);
        m_outputBuffer.retrieve(written);

        if (m_outputBuffer.readableBytes() == 0)
        {
            if (m_writeCompleteCallback)
            {
                m_writeCompleteCallback(shared_from_this());
            }

            if (m_state == State::Disconnecting)
            {
                m_socket.shutdownWrite();
            }
        }

        return written;
    }

    if (n < 0)
    {
        const int err = lastSocketErrorCode();
        if (isWouldBlockError(err))
        {
            return 0;
        }

        handleError(err);
    }

    return 0;
}

void TcpConnection::handleClose()
{
    if (m_state == State::Disconnected)
    {
        return;
    }

    m_state = State::Disconnected;

    if (m_closeCallback)
    {
        m_closeCallback(shared_from_this());
    }
}

void TcpConnection::handleError(int err)
{
    if (m_errorCallback)
    {
        m_errorCallback(shared_from_this(), err);
    }
}

}  // namespace dbase::net