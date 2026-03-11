#include "dbase/log/log.h"
#include "dbase/net/buffer.h"
#include "dbase/net/event_loop.h"
#include "dbase/net/socket_ops.h"
#include "dbase/net/tcp_client.h"
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
        dbase::net::TcpClient client(&loop, dbase::net::InetAddress("127.0.0.1", 9781), "echo-client");

        client.setConnectionCallback(
                [](const dbase::net::TcpConnection::Ptr& conn)
                {
                    DBASE_LOG_INFO(
                            "client connection {} state={} peer={} tid={}",
                            conn->name(),
                            static_cast<int>(conn->state()),
                            conn->peerAddress().toIpPort(),
                            dbase::thread::current_thread::tid());

                    if (conn->connected())
                    {
                        conn->send("hello-from-client");
                    }
                });

        client.setMessageCallback(
                [&loop](const dbase::net::TcpConnection::Ptr& conn, dbase::net::Buffer& buffer)
                {
                    const auto msg = buffer.retrieveAllAsString();
                    DBASE_LOG_INFO(
                            "client recv {} bytes from {} tid={}: {}",
                            msg.size(),
                            conn->name(),
                            dbase::thread::current_thread::tid(),
                            msg);

                    conn->shutdown();
                    loop.quit();
                });

        client.setWriteCompleteCallback(
                [](const dbase::net::TcpConnection::Ptr& conn)
                {
                    DBASE_LOG_INFO(
                            "client write complete: {} tid={}",
                            conn->name(),
                            dbase::thread::current_thread::tid());
                });

        dbase::thread::Thread quitter(
                [&loop](std::stop_token)
                {
                    dbase::thread::current_thread::sleepForMs(10000);
                    loop.quit();
                },
                "client-quitter");

        quitter.start();
        client.connect();
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