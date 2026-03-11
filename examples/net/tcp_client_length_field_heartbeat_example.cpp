#include "dbase/log/log.h"
#include "dbase/net/event_loop.h"
#include "dbase/net/length_field_codec.h"
#include "dbase/net/socket_ops.h"
#include "dbase/net/tcp_client.h"
#include "dbase/thread/current_thread.h"
#include "dbase/thread/thread.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

int main()
{
    dbase::log::setDefaultLevel(dbase::log::Level::Trace);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::Compact);

    const auto initRet = dbase::net::SocketOps::initialize();
    if (!initRet)
    {
        DBASE_LOG_ERROR("socket initialize failed: {}", initRet.error().message());
        return 1;
    }

    try
    {
        dbase::net::EventLoop loop;
        dbase::net::InetAddress serverAddr("127.0.0.1", 9782);

        auto codec = std::make_shared<dbase::net::LengthFieldCodec>(
                4,
                dbase::net::LengthFieldCodec::LengthMode::PayloadOnly,
                1024 * 1024);

        dbase::net::TcpClient client(&loop, serverAddr, "hb-frame-client");
        client.setLengthFieldCodec(codec);
        client.enableRetry(true);
        client.setRetryDelayMs(500, 10000);
        client.setHeartbeatInterval(std::chrono::seconds(5));
        client.setIdleTimeout(std::chrono::seconds(20));

        client.setConnectionCallback(
                [&loop](const dbase::net::TcpConnection::Ptr& conn)
                {
                    DBASE_LOG_INFO(
                            "client connection {} state={} peer={} tid={}",
                            conn->name(),
                            static_cast<int>(conn->state()),
                            conn->peerAddress().toIpPort(),
                            dbase::thread::current_thread::tid());

                    if (conn->connected())
                    {
                        const std::vector<std::string> messages = {
                                "hello",
                                "client-msg-1",
                                "client-msg-2"};

                        for (const auto& msg : messages)
                        {
                            DBASE_LOG_INFO("client send frame: {}", msg);
                            conn->sendFrame(msg);
                        }

                        loop.runAfter(std::chrono::seconds(60), [conn]()
                                      {
                        if (conn->connected())
                        {
                            DBASE_LOG_INFO("client timed shutdown");
                            conn->shutdown();
                        } });
                    }
                });

        client.setFrameMessageCallback(
                [](const dbase::net::TcpConnection::Ptr& conn, std::string&& msg)
                {
                    DBASE_LOG_INFO(
                            "client recv frame {} bytes from {} tid={}: {}",
                            msg.size(),
                            conn->name(),
                            dbase::thread::current_thread::tid(),
                            msg);

                    if (msg == "PING")
                    {
                        conn->sendFrame("PONG");
                        return;
                    }
                });

        client.setWriteCompleteCallback(
                [](const dbase::net::TcpConnection::Ptr& conn)
                {
                    DBASE_LOG_INFO(
                            "client write complete: {} tid={}",
                            conn->name(),
                            dbase::thread::current_thread::tid());
                });

        client.setHeartbeatCallback(
                [](const dbase::net::TcpConnection::Ptr& conn)
                {
                    DBASE_LOG_INFO(
                            "client send heartbeat idle={}ms conn={}",
                            conn->idleFor().count(),
                            conn->name());
                    conn->sendFrame("PING");
                });

        client.setIdleCallback(
                [](const dbase::net::TcpConnection::Ptr& conn)
                {
                    DBASE_LOG_WARN(
                            "client idle timeout close conn={} idle={}ms",
                            conn->name(),
                            conn->idleFor().count());
                    conn->forceClose();
                });

        client.connect();

        DBASE_LOG_INFO("tcp heartbeat frame client start, connect to {}", serverAddr.toIpPort());
        loop.loop();
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