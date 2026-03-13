#include "dbase/log/log.h"
#include "dbase/net/ikcp.h"
#include "dbase/net/inet_address.h"
#include "dbase/net/socket.h"
#include "dbase/net/socket_ops.h"
#include "dbase/time/time.h"

#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <WinSock2.h>
#include <WS2tcpip.h>
#else
#include <cerrno>
#include <sys/socket.h>
#include <sys/types.h>
#endif

namespace
{
constexpr std::uint32_t kConv = 10086;
constexpr std::uint32_t kToken = 0x12345678;
constexpr std::uint16_t kServerPort = 9000;
constexpr int kRecvBufferSize = 2048;
constexpr int kKcpRecvBufferSize = 4096;
constexpr int kExpectedMessages = 5;

[[nodiscard]] bool isWouldBlock()
{
#if defined(_WIN32)
    const int err = ::WSAGetLastError();
    return err == WSAEWOULDBLOCK;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
}

struct ServerContext
{
        dbase::net::Socket* socket{nullptr};
        dbase::net::InetAddress peer;
        bool hasPeer{false};
};

int udpOutput(const char* buf, int len, ikcpcb*, void* user)
{
    auto* ctx = static_cast<ServerContext*>(user);
    if (ctx == nullptr || ctx->socket == nullptr || !ctx->socket->valid() || !ctx->hasPeer)
    {
        return -1;
    }

    const int sent = static_cast<int>(
            ::sendto(
                    ctx->socket->fd(),
                    buf,
                    len,
                    0,
                    ctx->peer.getSockAddr(),
                    ctx->peer.length()));

    if (sent < 0)
    {
        DBASE_LOG_ERROR("server sendto failed");
        return -1;
    }

    return sent;
}

void drainKcpRecv(ikcpcb* kcp, int& recvCount)
{
    for (;;)
    {
        std::array<char, kKcpRecvBufferSize> buffer{};
        const int n = ikcp_recv(kcp, buffer.data(), static_cast<int>(buffer.size()));
        if (n < 0)
        {
            break;
        }

        std::string message(buffer.data(), static_cast<std::size_t>(n));
        DBASE_LOG_INFO("server recv: {}", message);

        const std::string reply = "echo: " + message;
        const int ret = ikcp_send(kcp, reply.data(), static_cast<int>(reply.size()));
        if (ret < 0)
        {
            DBASE_LOG_ERROR("server ikcp_send failed, ret={}", ret);
            continue;
        }

        ++recvCount;
    }
}
}  // namespace

int main()
{
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::SourceFunction);
    dbase::log::setDefaultLevel(dbase::log::Level::Info);

    auto initRet = dbase::net::SocketOps::initialize();
    if (!initRet)
    {
        DBASE_LOG_FATAL("SocketOps initialize failed: {}", initRet.error().message());
        return 1;
    }

    auto socket = dbase::net::Socket::createUdp(AF_INET);
    if (!socket.valid())
    {
        DBASE_LOG_FATAL("create udp socket failed");
        dbase::net::SocketOps::cleanup();
        return 1;
    }

    socket.setNonBlock(true);
    socket.setReuseAddr(true);

    dbase::net::InetAddress listenAddr(kServerPort, false, false);
    socket.bindAddress(listenAddr);

    ServerContext context;
    context.socket = &socket;

    ikcpcb* kcp = ikcp_create(kConv, &context);
    if (kcp == nullptr)
    {
        DBASE_LOG_FATAL("ikcp_create failed");
        dbase::net::SocketOps::cleanup();
        return 1;
    }

    ikcp_settoken(kcp, kToken);
    ikcp_setoutput(kcp, udpOutput);
    ikcp_wndsize(kcp, 128, 128);
    ikcp_nodelay(kcp, 1, 20, 2, 1);

    DBASE_LOG_INFO("kcp server listen on {}", listenAddr.toIpPort());

    int recvCount = 0;
    std::uint32_t nextUpdate = static_cast<std::uint32_t>(dbase::time::steadyNowMs());

    while (recvCount < kExpectedMessages)
    {
        std::array<char, kRecvBufferSize> udpBuffer{};

        for (;;)
        {
            sockaddr_storage peerStorage{};
            socklen_t peerLen = static_cast<socklen_t>(sizeof(peerStorage));

            const int n = static_cast<int>(
                    ::recvfrom(
                            socket.fd(),
                            udpBuffer.data(),
                            static_cast<int>(udpBuffer.size()),
                            0,
                            reinterpret_cast<sockaddr*>(&peerStorage),
                            &peerLen));

            if (n < 0)
            {
                if (isWouldBlock())
                {
                    break;
                }

                DBASE_LOG_ERROR("server recvfrom failed");
                ikcp_release(kcp);
                dbase::net::SocketOps::cleanup();
                return 1;
            }

            dbase::net::InetAddress peerAddr(reinterpret_cast<sockaddr*>(&peerStorage), peerLen);
            if (!context.hasPeer)
            {
                context.peer = peerAddr;
                context.hasPeer = true;
                DBASE_LOG_INFO("server peer set to {}", context.peer.toIpPort());
            }

            const int ret = ikcp_input(kcp, udpBuffer.data(), n);
            if (ret < 0)
            {
                DBASE_LOG_WARN("server ikcp_input failed, ret={}", ret);
            }
        }

        const auto now = static_cast<std::uint32_t>(dbase::time::steadyNowMs());
        if (static_cast<std::int32_t>(now - nextUpdate) >= 0)
        {
            ikcp_update(kcp, now);
            nextUpdate = ikcp_check(kcp, now);
        }

        drainKcpRecv(kcp, recvCount);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    for (int i = 0; i < 200; ++i)
    {
        const auto now = static_cast<std::uint32_t>(dbase::time::steadyNowMs());
        ikcp_update(kcp, now);

        std::array<char, kRecvBufferSize> udpBuffer{};
        for (;;)
        {
            sockaddr_storage peerStorage{};
            socklen_t peerLen = static_cast<socklen_t>(sizeof(peerStorage));
            const int n = static_cast<int>(
                    ::recvfrom(
                            socket.fd(),
                            udpBuffer.data(),
                            static_cast<int>(udpBuffer.size()),
                            0,
                            reinterpret_cast<sockaddr*>(&peerStorage),
                            &peerLen));

            if (n < 0)
            {
                if (isWouldBlock())
                {
                    break;
                }
                break;
            }

            (void)ikcp_input(kcp, udpBuffer.data(), n);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    DBASE_LOG_INFO(
            "server stats: rtt={}ms pktloss={}%% tx_bw={}B/s rx_bw={}B/s",
            ikcp_rtt(kcp),
            ikcp_pktloss(kcp),
            ikcp_tx_bandwidth(kcp),
            ikcp_rx_bandwidth(kcp));

    ikcp_release(kcp);
    dbase::net::SocketOps::cleanup();
    return 0;
}