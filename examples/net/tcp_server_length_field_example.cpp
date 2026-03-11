#include "dbase/log/log.h"
#include "dbase/net/event_loop.h"
#include "dbase/net/length_field_codec.h"
#include "dbase/net/socket_ops.h"
#include "dbase/net/tcp_server.h"
#include "dbase/thread/current_thread.h"
#include "dbase/thread/thread.h"

#include <memory>

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
        dbase::net::InetAddress listenAddr(9781, true, false);

        auto codec = std::make_shared<dbase::net::LengthFieldCodec>(
                4,
                dbase::net::LengthFieldCodec::LengthMode::PayloadOnly,
                1024 * 1024);

        dbase::net::TcpServer server(&loop, listenAddr, "frame-echo-server", false, false);
        server.setThreadCount(3);
        server.setLengthFieldCodec(codec);

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
                    conn->sendFrame(msg);
                });

        server.setWriteCompleteCallback(
                [](const dbase::net::TcpConnection::Ptr& conn)
                {
                    DBASE_LOG_INFO(
                            "write complete: {} tid={}",
                            conn->name(),
                            dbase::thread::current_thread::tid());
                });

        server.start();
        DBASE_LOG_INFO("tcp frame server listen on {}", listenAddr.toIpPort());

        dbase::thread::Thread quitter(
                [&loop](std::stop_token)
                {
                    dbase::thread::current_thread::sleepForMs(30000);
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