#pragma once

#include "dbase/net/buffer.h"
#include "dbase/net/channel.h"
#include "dbase/net/event_loop.h"
#include "dbase/net/inet_address.h"
#include "dbase/net/length_field_codec.h"
#include "dbase/net/socket.h"

#include <any>
#include <atomic>
#include <chrono>
#include <cstddef>
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

        enum class OutputOverflowPolicy
        {
            Ignore,
            CloseConnection,
            ReportError
        };

        static constexpr int kCodecError = -1001;
        static constexpr int kOutputBufferOverflowError = -1002;

        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;
        using Ptr = std::shared_ptr<TcpConnection>;
        using ConnectionCallback = std::function<void(const Ptr&)>;
        using MessageCallback = std::function<void(const Ptr&, Buffer&)>;
        using FrameMessageCallback = std::function<void(const Ptr&, std::string&&)>;
        using WriteCompleteCallback = std::function<void(const Ptr&)>;
        using CloseCallback = std::function<void(const Ptr&)>;
        using ErrorCallback = std::function<void(const Ptr&, int)>;
        using HighWaterMarkCallback = std::function<void(const Ptr&, std::size_t)>;

        TcpConnection(
                EventLoop* loop,
                std::string name,
                Socket socket,
                const InetAddress& localAddr,
                const InetAddress& peerAddr);

        TcpConnection(const TcpConnection&) = delete;
        TcpConnection& operator=(const TcpConnection&) = delete;

        TcpConnection(TcpConnection&&) = delete;
        TcpConnection& operator=(TcpConnection&&) = delete;

        ~TcpConnection();

        [[nodiscard]] EventLoop* ownerLoop() const noexcept;
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

        [[nodiscard]] std::size_t maxOutputBufferBytes() const noexcept;
        [[nodiscard]] OutputOverflowPolicy outputOverflowPolicy() const noexcept;

        [[nodiscard]] TimePoint connectedAt() const noexcept;
        [[nodiscard]] TimePoint lastActiveAt() const noexcept;
        [[nodiscard]] TimePoint lastProbeAt() const noexcept;
        [[nodiscard]] std::chrono::milliseconds idleFor() const noexcept;
        [[nodiscard]] std::chrono::milliseconds probeIdleFor() const noexcept;

        void setConnectionCallback(ConnectionCallback cb);
        void setMessageCallback(MessageCallback cb);
        void setFrameMessageCallback(FrameMessageCallback cb);
        void setWriteCompleteCallback(WriteCompleteCallback cb);
        void setCloseCallback(CloseCallback cb);
        void setErrorCallback(ErrorCallback cb);
        void setHighWaterMarkCallback(HighWaterMarkCallback cb);

        void setLengthFieldCodec(std::shared_ptr<LengthFieldCodec> codec);
        [[nodiscard]] const std::shared_ptr<LengthFieldCodec>& codec() const noexcept;

        void setMaxOutputBufferBytes(std::size_t bytes) noexcept;
        void setOutputOverflowPolicy(OutputOverflowPolicy policy) noexcept;

        void setContext(std::any context);
        [[nodiscard]] const std::any& context() const noexcept;
        [[nodiscard]] std::any& context() noexcept;
        void clearContext() noexcept;

        template <typename T>
        [[nodiscard]] const T* contextAs() const noexcept
        {
            return std::any_cast<T>(&m_context);
        }

        template <typename T>
        [[nodiscard]] T* contextAs() noexcept
        {
            return std::any_cast<T>(&m_context);
        }

        void connectEstablished();
        void connectDestroyed();

        void send(std::string_view data);
        void send(Buffer& buffer);
        void sendFrame(std::string_view payload);

        void shutdown();
        void forceClose();

        void markKeepAliveProbeSent() noexcept;

    private:
        static std::int64_t toTick(TimePoint tp) noexcept;
        static TimePoint fromTick(std::int64_t tick) noexcept;

        void sendInLoop(std::string data);
        void shutdownInLoop();
        void forceCloseInLoop();

        void handleRead();
        void handleWrite();
        void handleClose();
        void handleError();
        void handleCodecMessages();

        [[nodiscard]] bool handleOutputBufferAppend(std::string_view data);
        void notifyHighWaterMark(std::size_t bytes);
        void handleOutputOverflow();
        void touchActive() noexcept;

        void setState(State state) noexcept;

    private:
        EventLoop* m_loop{nullptr};
        std::string m_name;
        State m_state{State::Connecting};
        Socket m_socket;
        InetAddress m_localAddr;
        InetAddress m_peerAddr;
        std::unique_ptr<Channel> m_channel;
        Buffer m_inputBuffer;
        Buffer m_outputBuffer;

        std::shared_ptr<LengthFieldCodec> m_codec;

        std::size_t m_maxOutputBufferBytes{64 * 1024 * 1024};
        OutputOverflowPolicy m_outputOverflowPolicy{OutputOverflowPolicy::CloseConnection};

        std::atomic<std::int64_t> m_connectedAtTick{0};
        std::atomic<std::int64_t> m_lastActiveAtTick{0};
        std::atomic<std::int64_t> m_lastProbeAtTick{0};

        std::any m_context;

        ConnectionCallback m_connectionCallback;
        MessageCallback m_messageCallback;
        FrameMessageCallback m_frameMessageCallback;
        WriteCompleteCallback m_writeCompleteCallback;
        CloseCallback m_closeCallback;
        ErrorCallback m_errorCallback;
        HighWaterMarkCallback m_highWaterMarkCallback;
};
}  // namespace dbase::net