#pragma once

#include "dbase/net/buffer.h"
#include "dbase/net/inet_address.h"
#include "dbase/net/socket.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace dbase::net
{
class TcpConnection : public std::enable_shared_from_this<TcpConnection>
{
    public:
        enum class State
        {
            Disconnected,
            Connecting,
            Connected,
            Disconnecting
        };

        using Ptr = std::shared_ptr<TcpConnection>;
        using MessageCallback = std::function<void(const Ptr&, Buffer&)>;
        using WriteCompleteCallback = std::function<void(const Ptr&)>;
        using CloseCallback = std::function<void(const Ptr&)>;
        using ErrorCallback = std::function<void(const Ptr&, int)>;

        TcpConnection(
                std::string name,
                Socket socket,
                const InetAddress& localAddr,
                const InetAddress& peerAddr);

        TcpConnection(const TcpConnection&) = delete;
        TcpConnection& operator=(const TcpConnection&) = delete;

        TcpConnection(TcpConnection&&) = delete;
        TcpConnection& operator=(TcpConnection&&) = delete;

        ~TcpConnection() = default;

        [[nodiscard]] const std::string& name() const noexcept;
        [[nodiscard]] State state() const noexcept;
        [[nodiscard]] bool connected() const noexcept;
        [[nodiscard]] bool disconnected() const noexcept;

        [[nodiscard]] const InetAddress& localAddress() const noexcept;
        [[nodiscard]] const InetAddress& peerAddress() const noexcept;

        [[nodiscard]] SocketType fd() const noexcept;

        [[nodiscard]] Buffer& inputBuffer() noexcept;
        [[nodiscard]] Buffer& outputBuffer() noexcept;
        [[nodiscard]] const Buffer& inputBuffer() const noexcept;
        [[nodiscard]] const Buffer& outputBuffer() const noexcept;

        void setMessageCallback(MessageCallback cb);
        void setWriteCompleteCallback(WriteCompleteCallback cb);
        void setCloseCallback(CloseCallback cb);
        void setErrorCallback(ErrorCallback cb);

        void connectEstablished();
        void connectDestroyed();

        void send(std::string_view data);
        void send(Buffer& buffer);
        void shutdown();
        void forceClose();

        [[nodiscard]] std::size_t receiveOnce(std::size_t maxBytes = 64 * 1024);
        [[nodiscard]] std::size_t flushOutput();

    private:
        void handleClose();
        void handleError(int err);

    private:
        std::string m_name;
        State m_state{State::Connecting};
        Socket m_socket;
        InetAddress m_localAddr;
        InetAddress m_peerAddr;
        Buffer m_inputBuffer;
        Buffer m_outputBuffer;

        MessageCallback m_messageCallback;
        WriteCompleteCallback m_writeCompleteCallback;
        CloseCallback m_closeCallback;
        ErrorCallback m_errorCallback;
};
}  // namespace dbase::net