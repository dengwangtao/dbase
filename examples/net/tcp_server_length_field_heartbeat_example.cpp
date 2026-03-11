#include "dbase/log/log.h"
#include "dbase/net/event_loop.h"
#include "dbase/net/length_field_codec.h"
#include "dbase/net/socket_ops.h"
#include "dbase/net/tcp_server.h"
#include "dbase/thread/current_thread.h"
#include "dbase/thread/thread.h"

#include <memory>
#include <string_view>

int main()
{
    dbase::log::setDefaultLevel(dbase::log::Level::Trace);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::Source);

    const auto initRet = dbase::net::SocketOps::initialize();
    if (!initRet)
    {
        DBASE_LOG_ERROR("socket initialize failed: {}", initRet.error().message());
        return 1;
    }

    try
    {
        dbase::net::EventLoop loop;
        dbase::net::InetAddress listenAddr(9782, true, false);

        auto codec = std::make_shared<dbase::net::LengthFieldCodec>(
                4,
                dbase::net::LengthFieldCodec::LengthMode::PayloadOnly,
                1024 * 1024);

        dbase::net::TcpServer server(&loop, listenAddr, "hb-frame-server", false, false);
        server.setThreadCount(2);
        server.setLengthFieldCodec(codec);
        server.setHeartbeatInterval(std::chrono::seconds(5));
        server.setIdleTimeout(std::chrono::seconds(15));

        server.setThreadInitCallback([]()
                                     { DBASE_LOG_INFO("io loop init, tid={}", dbase::thread::current_thread::tid()); });

        server.setConnectionCallback(
                [](const dbase::net::TcpConnection::Ptr& conn)
                {
                    DBASE_LOG_INFO(
                            "connection {} state={} peer={} io_tid={}",
                            conn->name(),
                            static_cast<int>(conn->state()),
                            conn->peerAddress().toIpPort(),
                            dbase::thread::current_thread::tid());
                });

        server.setFrameMessageCallback(
                [](const dbase::net::TcpConnection::Ptr& conn, std::string&& msg)
                {
                    DBASE_LOG_INFO(
                            "frame recv {} bytes from {} on tid={}: {}",
                            msg.size(),
                            conn->name(),
                            dbase::thread::current_thread::tid(),
                            msg);

                    if (msg == "PING")
                    {
                        conn->sendFrame("PONG");
                        return;
                    }

                    conn->sendFrame(msg);
                });

        server.setHeartbeatCallback(
                [](const dbase::net::TcpConnection::Ptr& conn)
                {
                    DBASE_LOG_INFO(
                            "send heartbeat to {} idle={}ms tid={}",
                            conn->name(),
                            conn->idleFor().count(),
                            dbase::thread::current_thread::tid());
                    conn->sendFrame("PING");
                });

        server.setIdleCallback(
                [](const dbase::net::TcpConnection::Ptr& conn)
                {
                    DBASE_LOG_WARN(
                            "idle timeout close {} idle={}ms tid={}",
                            conn->name(),
                            conn->idleFor().count(),
                            dbase::thread::current_thread::tid());
                    conn->forceClose();
                });

        server.start();
        DBASE_LOG_INFO("tcp heartbeat frame server listen on {}", listenAddr.toIpPort());

        dbase::thread::Thread quitter(
                [&loop](std::stop_token)
                {
                    dbase::thread::current_thread::sleepForMs(6000000);
                    loop.quit();
                },
                "server-quitter");

        quitter.start();
        loop.loop();
        quitter.join();
    }
    catch (const std::exception& ex)
    {
        DBASE_LOG_ERROR("exception: {}", ex.what());
        dbase::net::SocketOps::cleanup();
        return 1;
    }

    dbase::net::SocketOps::cleanup();
    return 0;
}