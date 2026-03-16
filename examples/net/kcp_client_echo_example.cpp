#include "dbase/log/log.h"
#include "dbase/net/event_loop.h"
#include "dbase/net/inet_address.h"
#include "dbase/net/kcp_client.h"
#include "dbase/net/socket_ops.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <span>
#include <string>
#include <string_view>

namespace
{
std::atomic<bool> g_stop{false};

void signalHandler(int)
{
    g_stop.store(true, std::memory_order_release);
}

std::string toString(std::span<const std::byte> data)
{
    return std::string(
            reinterpret_cast<const char*>(data.data()),
            data.size());
}
}  // namespace

int main(int argc, char* argv[])
{
    auto initRet = dbase::net::SocketOps::initialize();
    if (!initRet)
    {
        std::fprintf(stderr, "Socket initialize failed: %s\n", initRet.error().message().c_str());
        return 1;
    }

    std::signal(SIGINT, signalHandler);
#if !defined(_WIN32)
    std::signal(SIGTERM, signalHandler);
#endif

    dbase::log::setDefaultLevel(dbase::log::Level::Info);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::SourceFunction);

    std::string ip = "127.0.0.1";
    std::uint16_t port = 9982;
    std::uint32_t conv = 10086;
    std::uint32_t token = 123456;

    if (argc >= 2)
    {
        ip = argv[1];
    }
    if (argc >= 3)
    {
        port = static_cast<std::uint16_t>(std::strtoul(argv[2], nullptr, 10));
    }
    if (argc >= 4)
    {
        conv = static_cast<std::uint32_t>(std::strtoul(argv[3], nullptr, 10));
    }
    if (argc >= 5)
    {
        token = static_cast<std::uint32_t>(std::strtoul(argv[4], nullptr, 10));
    }

    dbase::net::EventLoop loop;
    dbase::net::InetAddress serverAddr(ip, port);

    dbase::net::KcpSession::Options options;
    options.conv = conv;
    options.token = token;
    options.mtu = 1400;
    options.sndWnd = 128;
    options.rcvWnd = 128;
    options.nodelay = 1;
    options.interval = 20;
    options.resend = 2;
    options.nc = 1;
    options.idleTimeout = std::chrono::seconds(30);
    options.maxMessageBytes = 4 * 1024 * 1024;

    dbase::net::KcpClient client(&loop, serverAddr, "kcp-echo-client", options);

    client.setConnectionCallback(
            [](const dbase::net::KcpClient::SessionPtr& session)
            {
                DBASE_LOG_INFO(
                        "client session established, peer={}, conv={}, token={}",
                        session->peerAddress().toIpPort(),
                        session->conv(),
                        session->token());
            });

    client.setMessageCallback(
            [](const dbase::net::KcpClient::SessionPtr& session, std::span<const std::byte> data)
            {
                const auto text = toString(data);
                DBASE_LOG_INFO(
                        "client recv echo, peer={}, conv={}, bytes={}, text={}, rtt={}ms, pktloss={}%, tx={}B/s, rx={}B/s",
                        session->peerAddress().toIpPort(),
                        session->conv(),
                        data.size(),
                        text,
                        session->rtt(),
                        session->pktloss(),
                        session->txBandwidth(),
                        session->rxBandwidth());
            });

    client.setWriteCompleteCallback(
            [](const dbase::net::KcpClient::SessionPtr& session)
            {
                DBASE_LOG_INFO(
                        "client write complete, peer={}, conv={}, rtt={}ms, pktloss={}%, tx={}B/s, rx={}B/s",
                        session->peerAddress().toIpPort(),
                        session->conv(),
                        session->rtt(),
                        session->pktloss(),
                        session->txBandwidth(),
                        session->rxBandwidth());
            });

    client.setCloseCallback(
            [&loop](const dbase::net::KcpClient::SessionPtr& session)
            {
                DBASE_LOG_INFO(
                        "client session closed, peer={}, conv={}, token={}",
                        session->peerAddress().toIpPort(),
                        session->conv(),
                        session->token());
                loop.quit();
            });

    client.setErrorCallback(
            [](const dbase::net::KcpClient::SessionPtr& session, const dbase::Error& error)
            {
                DBASE_LOG_WARN(
                        "client session error, peer={}, conv={}, token={}, error={}",
                        session->peerAddress().toIpPort(),
                        session->conv(),
                        session->token(),
                        error.message());
            });

    client.connect();

    std::uint64_t seq = 0;
    loop.runEvery(
            std::chrono::seconds(1),
            [&client, &seq]()
            {
                auto session = client.session();
                if (!session || !session->connected())
                {
                    DBASE_LOG_WARN("client session not ready");
                    return;
                }

                const std::string payload =
                        "hello from kcp client, seq=" + std::to_string(seq++) + ", ts_ms=" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());

                DBASE_LOG_INFO(
                        "client send, peer={}, conv={}, bytes={}, text={}",
                        session->peerAddress().toIpPort(),
                        session->conv(),
                        payload.size(),
                        payload);

                session->send(payload);
            });

    loop.runEvery(
            std::chrono::milliseconds(200),
            [&loop, &client]()
            {
                if (!g_stop.load(std::memory_order_acquire))
                {
                    return;
                }

                auto session = client.session();
                if (session)
                {
                    DBASE_LOG_INFO("client stopping...");
                    session->forceClose();
                }
                else
                {
                    loop.quit();
                }
            });

    DBASE_LOG_INFO(
            "kcp echo client start, server={}, conv={}, token={}",
            serverAddr.toIpPort(),
            conv,
            token);

    loop.loop();

    dbase::net::SocketOps::cleanup();
    return 0;
}