#pragma once
#include "dbase/error/error.h"
#include "dbase/net/event_loop.h"
#include "dbase/net/ikcp.h"
#include "dbase/net/inet_address.h"
#include "dbase/net/udp_socket.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace dbase::net
{
class KcpSession : public std::enable_shared_from_this<KcpSession>
{
    public:
        using Ptr = std::shared_ptr<KcpSession>;
        using MessageCallback = std::function<void(const Ptr&, std::span<const std::byte>)>;
        using WriteCompleteCallback = std::function<void(const Ptr&)>;
        using CloseCallback = std::function<void(const Ptr&)>;
        using ErrorCallback = std::function<void(const Ptr&, const dbase::Error&)>;

        struct Options
        {
                std::uint32_t conv{0};
                std::uint32_t token{0};
                int mtu{1400};
                int sndWnd{128};
                int rcvWnd{128};
                int nodelay{1};
                int interval{20};
                int resend{2};
                int nc{1};
                std::chrono::milliseconds idleTimeout{0};
                std::size_t maxMessageBytes{16 * 1024 * 1024};
        };

        KcpSession(
                EventLoop* loop,
                UdpSocket* udpSocket,
                InetAddress peerAddr,
                Options options);
        ~KcpSession();
        KcpSession(const KcpSession&) = delete;
        KcpSession& operator=(const KcpSession&) = delete;

        void connectEstablished();
        void connectDestroyed();
        void send(std::span<const std::byte> data);
        void send(std::string_view data);
        void inputPacket(std::span<const std::byte> packet);
        void shutdown();
        void forceClose();

        [[nodiscard]] EventLoop* ownerLoop() const noexcept;
        [[nodiscard]] const InetAddress& peerAddress() const noexcept;
        [[nodiscard]] std::uint32_t conv() const noexcept;
        [[nodiscard]] std::uint32_t token() const noexcept;
        [[nodiscard]] bool connected() const noexcept;
        [[nodiscard]] std::chrono::steady_clock::time_point lastActiveAt() const noexcept;
        [[nodiscard]] std::chrono::milliseconds idleFor() const noexcept;

        void setMessageCallback(MessageCallback cb);
        void setWriteCompleteCallback(WriteCompleteCallback cb);
        void setCloseCallback(CloseCallback cb);
        void setErrorCallback(ErrorCallback cb);

        [[nodiscard]] std::uint32_t rtt() const noexcept;
        [[nodiscard]] std::uint32_t pktloss() const noexcept;
        [[nodiscard]] std::uint32_t txBandwidth() const noexcept;
        [[nodiscard]] std::uint32_t rxBandwidth() const noexcept;

    private:
        enum class State
        {
            Connecting,
            Connected,
            Disconnecting,
            Disconnected
        };

        static int kcpOutput(const char* buf, int len, ikcpcb* kcp, void* user);

        void sendInLoop(std::vector<std::byte> data);
        void inputPacketInLoop(std::vector<std::byte> packet);
        void updateInLoop();
        void scheduleUpdate(std::uint32_t nowMs);
        void cancelUpdateTimer();
        void drainRecvQueue();
        void handleError(dbase::Error error);
        void handleClose();
        void touchActive() noexcept;
        void refreshStats() noexcept;
        [[nodiscard]] static std::uint32_t nowMs() noexcept;

    private:
        EventLoop* m_loop{nullptr};
        UdpSocket* m_udpSocket{nullptr};
        InetAddress m_peerAddr;
        Options m_options;
        ikcpcb* m_kcp{nullptr};
        std::atomic<State> m_state{State::Connecting};
        EventLoop::TimerId m_updateTimerId{0};
        MessageCallback m_messageCallback;
        WriteCompleteCallback m_writeCompleteCallback;
        CloseCallback m_closeCallback;
        ErrorCallback m_errorCallback;
        std::atomic<std::int64_t> m_lastActiveTickMs{0};
        std::atomic<std::uint32_t> m_rtt{0};
        std::atomic<std::uint32_t> m_pktloss{0};
        std::atomic<std::uint32_t> m_txBandwidth{0};
        std::atomic<std::uint32_t> m_rxBandwidth{0};
};
}  // namespace dbase::net