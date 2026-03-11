#include "dbase/log/log.h"
#include "dbase/net/event_loop.h"
#include "dbase/net/framed_tcp_server.h"
#include "dbase/net/length_field_codec.h"
#include "dbase/net/socket_ops.h"
#include "dbase/thread/current_thread.h"
#include "dbase/thread/thread.h"

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
        dbase::net::InetAddress listenAddr(9860, true, false);

        dbase::net::LengthFieldCodec codec(
            4,
            dbase::net::LengthFieldCodec::LengthMode::PayloadOnly,
            16 * 1024 * 1024);

        dbase::net::FramedTcpServer server(
            &loop,
            listenAddr,
            "framed-echo-server",
            codec,
            false,
            false);

        server.setThreadCount(3);

        server.setThreadInitCallback([]() {
            DBASE_LOG_INFO("io loop init tid={}", dbase::thread::current_thread::tid());
        });

        server.setConnectionCallback(
            [](const dbase::net::TcpConnection::Ptr& conn) {
                DBASE_LOG_INFO(
                    "connection {} state={} peer={} io_tid={}",
                    conn->name(),
                    static_cast<int>(conn->state()),
                    conn->peerAddress().toIpPort(),
                    dbase::thread::current_thread::tid());
            });

        server.setFrameMessageCallback(
            [&server](const dbase::net::TcpConnection::Ptr& conn, std::string&& payload) {
                DBASE_LOG_INFO(
                    "frame recv {} bytes from {} on tid={}: {}",
                    payload.size(),
                    conn->name(),
                    dbase::thread::current_thread::tid(),
                    payload);

                server.send(conn, payload);
            });

        server.setWriteCompleteCallback(
            [](const dbase::net::TcpConnection::Ptr& conn) {
                DBASE_LOG_INFO(
                    "frame write complete: {} tid={}",
                    conn->name(),
                    dbase::thread::current_thread::tid());
            });

        server.start();
        DBASE_LOG_INFO("framed tcp server listen on {}", listenAddr.toIpPort());

        dbase::thread::Thread quitter(
            [&loop](std::stop_token) {
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