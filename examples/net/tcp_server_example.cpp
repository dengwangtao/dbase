#include "dbase/log/log.h"
#include "dbase/net/buffer.h"
#include "dbase/net/event_loop.h"
#include "dbase/net/socket_ops.h"
#include "dbase/net/tcp_server.h"
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
        dbase::net::InetAddress listenAddr(9781, true, false);

        dbase::net::TcpServer server(&loop, listenAddr, "echo-server", false, false);
        server.setThreadCount(3);

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

        server.setMessageCallback(
                [](const dbase::net::TcpConnection::Ptr& conn, dbase::net::Buffer& buffer)
                {
                    const auto msg = buffer.retrieveAllAsString();
                    DBASE_LOG_INFO(
                            "recv {} bytes from {} on tid={}: {}",
                            msg.size(),
                            conn->name(),
                            dbase::thread::current_thread::tid(),
                            msg);
                    conn->send(msg);
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
        DBASE_LOG_INFO("tcp server listen on {}", listenAddr.toIpPort());

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

/*
$client = New-Object System.Net.Sockets.TcpClient
$client.Connect("127.0.0.1", 9781)
$stream = $client.GetStream()
$bytes = [System.Text.Encoding]::UTF8.GetBytes("hello`n")
$stream.Write($bytes, 0, $bytes.Length)
$buf = New-Object byte[] 1024
$n = $stream.Read($buf, 0, $buf.Length)
[System.Text.Encoding]::UTF8.GetString($buf, 0, $n)
$client.Close()
*/