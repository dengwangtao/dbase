#include "dbase/log/log.h"
#include "dbase/net/event_loop.h"
#include "dbase/net/length_field_codec.h"
#include "dbase/net/socket_ops.h"
#include "dbase/net/tcp_client.h"
#include "dbase/thread/current_thread.h"
#include "dbase/thread/thread.h"

#include <memory>
#include <string>
#include <vector>

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
        dbase::net::InetAddress serverAddr("127.0.0.1", 9781);

        auto codec = std::make_shared<dbase::net::LengthFieldCodec>(
                4,
                dbase::net::LengthFieldCodec::LengthMode::PayloadOnly,
                1024 * 1024);

        dbase::net::TcpClient client(&loop, serverAddr, "frame-client");
        client.setLengthFieldCodec(codec);
        client.enableRetry(false);

        client.setConnectionCallback(
                [](const dbase::net::TcpConnection::Ptr& conn)
                {
                    DBASE_LOG_INFO(
                            "client connection {} state={} peer={} tid={}",
                            conn->name(),
                            static_cast<int>(conn->state()),
                            conn->peerAddress().toIpPort(),
                            dbase::thread::current_thread::tid());
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
                });

        client.setWriteCompleteCallback(
                [](const dbase::net::TcpConnection::Ptr& conn)
                {
                    DBASE_LOG_INFO(
                            "client write complete: {} tid={}",
                            conn->name(),
                            dbase::thread::current_thread::tid());
                });

        client.connect();

        dbase::thread::Thread sender(
                [&loop, &client](std::stop_token)
                {
                    dbase::thread::current_thread::sleepForMs(1000);

                    loop.queueInLoop([&client]()
                                     {
                    auto conn = client.connection();
                    if (!conn)
                    {
                        DBASE_LOG_WARN("client connection not ready");
                        return;
                    }

                    const std::vector<std::string> messages = {
                        "hello",
                        "frame-message-1",
                        "frame-message-2",
                        "goodbye"
                    };

                    for (const auto& msg : messages)
                    {
                        DBASE_LOG_INFO("client send frame: {}", msg);
                        conn->sendFrame(msg);
                    } });

                    dbase::thread::current_thread::sleepForMs(3000);

                    loop.queueInLoop([&client]()
                                     {
                    auto conn = client.connection();
                    if (conn)
                    {
                        conn->shutdown();
                    } });

                    dbase::thread::current_thread::sleepForMs(1000);
                    loop.quit();
                },
                "client-sender");

        sender.start();
        DBASE_LOG_INFO("tcp frame client start, connect to {}", serverAddr.toIpPort());
        loop.loop();
        sender.join();
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