#include "dbase/log/log.h"
#include "dbase/net/acceptor.h"
#include "dbase/net/channel.h"
#include "dbase/net/event_loop.h"
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
        dbase::net::InetAddress listenAddr(9670, true, false);
        dbase::net::Acceptor acceptor(listenAddr, false, false);

        acceptor.setNewConnectionCallback(
                [](dbase::net::Socket conn, const dbase::net::InetAddress& peerAddr)
                {
                    DBASE_LOG_INFO("accepted connection from {}", peerAddr.toIpPort());
                    conn.shutdownWrite();
                });

        acceptor.listen();

        dbase::net::Channel acceptChannel(&loop, acceptor.socket().fd());
        acceptChannel.setReadCallback([&loop, &acceptor]()
                                      {
            const auto accepted = acceptor.acceptAvailable(32);
            if (accepted > 0)
            {
                DBASE_LOG_INFO("accept batch={}", accepted);
            } });

        acceptChannel.enableReading();

        dbase::thread::Thread quitter(
                [&loop](std::stop_token)
                {
                    dbase::thread::current_thread::sleepForMs(15000);
                    loop.quit();
                    DBASE_LOG_INFO("quitting event loop");
                },
                "loop-quitter");

        quitter.start();

        DBASE_LOG_INFO("event loop listening on {}", listenAddr.toIpPort());
        loop.loop();

        quitter.join();
        dbase::net::SocketOps::cleanup();
        return 0;
    }
    catch (const std::exception& ex)
    {
        DBASE_LOG_ERROR("exception: {}", ex.what());
        dbase::net::SocketOps::cleanup();
        return 1;
    }
}