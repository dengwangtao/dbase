#include "dbase/log/log.h"
#include "dbase/net/event_loop.h"
#include "dbase/net/inet_address.h"
#include "dbase/net/kcp_server.h"
#include "dbase/net/socket_ops.h"

#include <atomic>
#include <csignal>
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

    if (argc >= 2)
    {
        ip = argv[1];
    }
    if (argc >= 3)
    {
        port = static_cast<std::uint16_t>(std::strtoul(argv[2], nullptr, 10));
    }

    dbase::net::EventLoop loop;
    dbase::net::InetAddress listenAddr(ip, port);

    dbase::net::KcpServer server(&loop, listenAddr, "kcp-echo-server");
    dbase::net::KcpServer::SessionOptions options;
    options.mtu = 1400;
    options.sndWnd = 128;
    options.rcvWnd = 128;
    options.nodelay = 1;
    options.interval = 20;
    options.resend = 2;
    options.nc = 1;
    options.idleTimeout = std::chrono::seconds(30);
    options.maxMessageBytes = 4 * 1024 * 1024;

    server.setThreadCount(0);
    server.setSessionOptions(options);

    server.setConnectionCallback(
            [](const dbase::net::KcpServer::SessionPtr& session)
            {
                DBASE_LOG_INFO(
                        "kcp session established, peer={}, conv={}, token={}",
                        session->peerAddress().toIpPort(),
                        session->conv(),
                        session->token());
            });

    server.setMessageCallback(
            [](const dbase::net::KcpServer::SessionPtr& session, std::span<const std::byte> data)
            {
                const auto text = toString(data);
                DBASE_LOG_INFO(
                        "server recv, peer={}, conv={}, bytes={}, text={}",
                        session->peerAddress().toIpPort(),
                        session->conv(),
                        data.size(),
                        text);

                session->send(data);
            });

    server.setWriteCompleteCallback(
            [](const dbase::net::KcpServer::SessionPtr& session)
            {
                DBASE_LOG_INFO(
                        "server write complete, peer={}, conv={}, waitsnd={}, rtt={}ms, pktloss={}%, tx={}B/s, rx={}B/s",
                        session->peerAddress().toIpPort(),
                        session->conv(),
                        0,
                        session->rtt(),
                        session->pktloss(),
                        session->txBandwidth(),
                        session->rxBandwidth());
            });

    server.setCloseCallback(
            [](const dbase::net::KcpServer::SessionPtr& session)
            {
                DBASE_LOG_INFO(
                        "kcp session closed, peer={}, conv={}, token={}",
                        session->peerAddress().toIpPort(),
                        session->conv(),
                        session->token());
            });

    server.setErrorCallback(
            [](const dbase::net::KcpServer::SessionPtr& session, const dbase::Error& error)
            {
                DBASE_LOG_WARN(
                        "kcp session error, peer={}, conv={}, token={}, error={}",
                        session->peerAddress().toIpPort(),
                        session->conv(),
                        session->token(),
                        error.message());
            });

    server.start();

    DBASE_LOG_INFO("kcp echo server listen on {}", listenAddr.toIpPort());

    loop.runEvery(
            std::chrono::milliseconds(200),
            [&loop]()
            {
                if (g_stop.load(std::memory_order_acquire))
                {
                    DBASE_LOG_INFO("server stopping...");
                    loop.quit();
                }
            });

    loop.loop();

    dbase::net::SocketOps::cleanup();
    return 0;
}