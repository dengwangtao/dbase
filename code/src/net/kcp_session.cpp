#include "dbase/net/kcp_session.h"
#include "dbase/log/log.h"
#include <algorithm>
#include <chrono>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace dbase::net
{
namespace
{
using Clock = std::chrono::steady_clock;

[[nodiscard]] std::int64_t toTickMs(Clock::time_point tp) noexcept
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(tp.time_since_epoch()).count();
}

[[nodiscard]] Clock::time_point fromTickMs(std::int64_t tick) noexcept
{
    return Clock::time_point(std::chrono::milliseconds(tick));
}
}  // namespace

KcpSession::KcpSession(
        EventLoop* loop,
        UdpSocket* udpSocket,
        InetAddress peerAddr,
        Options options)
    : m_loop(loop),
      m_udpSocket(udpSocket),
      m_peerAddr(std::move(peerAddr)),
      m_options(options)
{
    if (m_loop == nullptr)
    {
        throw std::invalid_argument("KcpSession loop is null");
    }
    if (m_udpSocket == nullptr)
    {
        throw std::invalid_argument("KcpSession udpSocket is null");
    }
    if (m_options.conv == 0)
    {
        throw std::invalid_argument("KcpSession conv must not be 0");
    }

    m_kcp = ikcp_create(m_options.conv, this);
    if (m_kcp == nullptr)
    {
        throw std::runtime_error("KcpSession ikcp_create failed");
    }

    if (m_options.token != 0)
    {
        if (ikcp_settoken(m_kcp, m_options.token) != 0)
        {
            ikcp_release(m_kcp);
            m_kcp = nullptr;
            throw std::runtime_error("KcpSession ikcp_settoken failed");
        }
    }

    if (ikcp_setmtu(m_kcp, m_options.mtu) != 0)
    {
        ikcp_release(m_kcp);
        m_kcp = nullptr;
        throw std::runtime_error("KcpSession ikcp_setmtu failed");
    }

    ikcp_wndsize(m_kcp, m_options.sndWnd, m_options.rcvWnd);
    ikcp_nodelay(m_kcp, m_options.nodelay, m_options.interval, m_options.resend, m_options.nc);
    ikcp_setoutput(m_kcp, &KcpSession::kcpOutput);
    touchActive();
    refreshStats();
}

KcpSession::~KcpSession()
{
    cancelUpdateTimer();
    if (m_kcp != nullptr)
    {
        ikcp_release(m_kcp);
        m_kcp = nullptr;
    }
}

void KcpSession::connectEstablished()
{
    m_loop->assertInLoopThread();
    m_state.store(State::Connected, std::memory_order_release);
    scheduleUpdate(nowMs());
}

void KcpSession::connectDestroyed()
{
    m_loop->assertInLoopThread();
    if (m_state.load(std::memory_order_acquire) == State::Disconnected)
    {
        return;
    }

    cancelUpdateTimer();
    m_state.store(State::Disconnected, std::memory_order_release);
}

void KcpSession::send(std::span<const std::byte> data)
{
    if (data.empty())
    {
        return;
    }

    std::vector<std::byte> copy(data.begin(), data.end());
    m_loop->runInLoop(
            [self = shared_from_this(), payload = std::move(copy)]() mutable
            {
                self->sendInLoop(std::move(payload));
            });
}

void KcpSession::send(std::string_view data)
{
    const auto* begin = reinterpret_cast<const std::byte*>(data.data());
    send(std::span<const std::byte>(begin, data.size()));
}

void KcpSession::inputPacket(std::span<const std::byte> packet)
{
    if (packet.empty())
    {
        return;
    }

    std::vector<std::byte> copy(packet.begin(), packet.end());
    m_loop->runInLoop(
            [self = shared_from_this(), payload = std::move(copy)]() mutable
            {
                self->inputPacketInLoop(std::move(payload));
            });
}

void KcpSession::shutdown()
{
    m_loop->runInLoop(
            [self = shared_from_this()]()
            {
                if (self->m_state.load(std::memory_order_acquire) == State::Disconnected)
                {
                    return;
                }

                self->m_state.store(State::Disconnecting, std::memory_order_release);
                if (self->m_kcp == nullptr || ikcp_waitsnd(self->m_kcp) == 0)
                {
                    self->handleClose();
                }
            });
}

void KcpSession::forceClose()
{
    m_loop->runInLoop(
            [self = shared_from_this()]()
            {
                self->handleClose();
            });
}

EventLoop* KcpSession::ownerLoop() const noexcept
{
    return m_loop;
}

const InetAddress& KcpSession::peerAddress() const noexcept
{
    return m_peerAddr;
}

std::uint32_t KcpSession::conv() const noexcept
{
    return m_options.conv;
}

std::uint32_t KcpSession::token() const noexcept
{
    return m_options.token;
}

bool KcpSession::connected() const noexcept
{
    return m_state.load(std::memory_order_acquire) == State::Connected;
}

std::chrono::steady_clock::time_point KcpSession::lastActiveAt() const noexcept
{
    return fromTickMs(m_lastActiveTickMs.load(std::memory_order_acquire));
}

std::chrono::milliseconds KcpSession::idleFor() const noexcept
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - lastActiveAt());
}

void KcpSession::setMessageCallback(MessageCallback cb)
{
    m_messageCallback = std::move(cb);
}

void KcpSession::setWriteCompleteCallback(WriteCompleteCallback cb)
{
    m_writeCompleteCallback = std::move(cb);
}

void KcpSession::setCloseCallback(CloseCallback cb)
{
    m_closeCallback = std::move(cb);
}

void KcpSession::setErrorCallback(ErrorCallback cb)
{
    m_errorCallback = std::move(cb);
}

std::uint32_t KcpSession::rtt() const noexcept
{
    return m_rtt.load(std::memory_order_acquire);
}

std::uint32_t KcpSession::pktloss() const noexcept
{
    return m_pktloss.load(std::memory_order_acquire);
}

std::uint32_t KcpSession::txBandwidth() const noexcept
{
    return m_txBandwidth.load(std::memory_order_acquire);
}

std::uint32_t KcpSession::rxBandwidth() const noexcept
{
    return m_rxBandwidth.load(std::memory_order_acquire);
}

int KcpSession::kcpOutput(const char* buf, int len, ikcpcb*, void* user)
{
    auto* self = static_cast<KcpSession*>(user);
    if (self == nullptr || self->m_udpSocket == nullptr || len <= 0)
    {
        return -1;
    }

    const auto* first = reinterpret_cast<const std::byte*>(buf);
    const auto result = self->m_udpSocket->sendTo(
            std::span<const std::byte>(first, static_cast<std::size_t>(len)),
            self->m_peerAddr);

    if (!result)
    {
        if (result.error().code() != dbase::ErrorCode::WouldBlock)
        {
            DBASE_LOG_WARN(
                    "KcpSession output failed, peer={}, error={}",
                    self->m_peerAddr.toIpPort(),
                    result.error().message());
        }
        return -1;
    }

    return static_cast<int>(result.value());
}

void KcpSession::sendInLoop(std::vector<std::byte> data)
{
    m_loop->assertInLoopThread();

    const auto state = m_state.load(std::memory_order_acquire);
    if (state == State::Disconnected || state == State::Disconnecting)
    {
        return;
    }

    if (data.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    {
        handleError(dbase::Error(dbase::ErrorCode::InvalidArgument, "KcpSession send payload too large"));
        return;
    }

    const int ret = ikcp_send(
            m_kcp,
            reinterpret_cast<const char*>(data.data()),
            static_cast<int>(data.size()));
    if (ret < 0)
    {
        handleError(dbase::Error(dbase::ErrorCode::IOError, "KcpSession ikcp_send failed"));
        return;
    }

    touchActive();
    const auto now = nowMs();
    ikcp_update(m_kcp, now);
    refreshStats();
    scheduleUpdate(now);
}

void KcpSession::inputPacketInLoop(std::vector<std::byte> packet)
{
    m_loop->assertInLoopThread();

    if (m_state.load(std::memory_order_acquire) == State::Disconnected)
    {
        return;
    }

    const int ret = ikcp_input(
            m_kcp,
            reinterpret_cast<const char*>(packet.data()),
            static_cast<long>(packet.size()));
    if (ret < 0)
    {
        handleError(dbase::Error(dbase::ErrorCode::ParseError, "KcpSession ikcp_input failed: " + std::to_string(ret)));
        return;
    }

    touchActive();
    drainRecvQueue();

    const auto now = nowMs();
    ikcp_update(m_kcp, now);
    refreshStats();
    scheduleUpdate(now);

    if (m_state.load(std::memory_order_acquire) == State::Disconnecting && ikcp_waitsnd(m_kcp) == 0)
    {
        handleClose();
    }
}

void KcpSession::updateInLoop()
{
    m_loop->assertInLoopThread();

    if (m_state.load(std::memory_order_acquire) == State::Disconnected)
    {
        return;
    }

    const auto now = nowMs();
    ikcp_update(m_kcp, now);
    refreshStats();
    drainRecvQueue();
    scheduleUpdate(now);

    if (m_options.idleTimeout.count() > 0 && idleFor() >= m_options.idleTimeout)
    {
        handleError(dbase::Error(dbase::ErrorCode::Timeout, "KcpSession idle timeout"));
        handleClose();
        return;
    }

    if (m_state.load(std::memory_order_acquire) == State::Disconnecting && ikcp_waitsnd(m_kcp) == 0)
    {
        handleClose();
    }
}

void KcpSession::scheduleUpdate(std::uint32_t nowMsValue)
{
    if (m_state.load(std::memory_order_acquire) == State::Disconnected)
    {
        return;
    }

    const std::uint32_t next = ikcp_check(m_kcp, nowMsValue);
    const auto delay = next <= nowMsValue
                               ? std::chrono::milliseconds(0)
                               : std::chrono::milliseconds(next - nowMsValue);

    cancelUpdateTimer();
    m_updateTimerId = m_loop->runAfter(
            delay,
            [self = shared_from_this()]()
            {
                self->updateInLoop();
            });
}

void KcpSession::cancelUpdateTimer()
{
    if (m_updateTimerId != 0)
    {
        m_loop->cancelTimer(m_updateTimerId);
        m_updateTimerId = 0;
    }
}

void KcpSession::drainRecvQueue()
{
    for (;;)
    {
        const int peek = ikcp_peeksize(m_kcp);
        if (peek < 0)
        {
            break;
        }

        if (peek > static_cast<int>(m_options.maxMessageBytes))
        {
            handleError(dbase::Error(dbase::ErrorCode::InvalidState, "KcpSession message too large"));
            handleClose();
            return;
        }

        std::vector<std::byte> payload(static_cast<std::size_t>(peek));
        const int n = ikcp_recv(
                m_kcp,
                reinterpret_cast<char*>(payload.data()),
                static_cast<int>(payload.size()));
        if (n < 0)
        {
            break;
        }

        touchActive();
        if (m_messageCallback)
        {
            m_messageCallback(shared_from_this(), std::span<const std::byte>(payload.data(), static_cast<std::size_t>(n)));
        }
    }

    if (m_writeCompleteCallback && ikcp_waitsnd(m_kcp) == 0)
    {
        m_writeCompleteCallback(shared_from_this());
    }
}

void KcpSession::handleError(dbase::Error error)
{
    if (m_errorCallback)
    {
        m_errorCallback(shared_from_this(), error);
    }
    else
    {
        DBASE_LOG_WARN(
                "KcpSession error, peer={}, conv={}, token={}, error={}",
                m_peerAddr.toIpPort(),
                m_options.conv,
                m_options.token,
                error.message());
    }
}

void KcpSession::handleClose()
{
    if (m_state.load(std::memory_order_acquire) == State::Disconnected)
    {
        return;
    }

    cancelUpdateTimer();
    m_state.store(State::Disconnected, std::memory_order_release);

    if (m_closeCallback)
    {
        m_closeCallback(shared_from_this());
    }
}

void KcpSession::touchActive() noexcept
{
    m_lastActiveTickMs.store(toTickMs(Clock::now()), std::memory_order_release);
}

void KcpSession::refreshStats() noexcept
{
    if (m_kcp == nullptr)
    {
        m_rtt.store(0, std::memory_order_release);
        m_pktloss.store(0, std::memory_order_release);
        m_txBandwidth.store(0, std::memory_order_release);
        m_rxBandwidth.store(0, std::memory_order_release);
        return;
    }

    m_rtt.store(ikcp_rtt(m_kcp), std::memory_order_release);
    m_pktloss.store(ikcp_pktloss(m_kcp), std::memory_order_release);
    m_txBandwidth.store(ikcp_tx_bandwidth(m_kcp), std::memory_order_release);
    m_rxBandwidth.store(ikcp_rx_bandwidth(m_kcp), std::memory_order_release);
}

std::uint32_t KcpSession::nowMs() noexcept
{
    return static_cast<std::uint32_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                    Clock::now().time_since_epoch())
                    .count());
}
}  // namespace dbase::net