#include "dbase/log/log.h"
#include "dbase/net/acceptor.h"
#include "dbase/net/socket_ops.h"
#include "dbase/thread/current_thread.h"

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
        dbase::net::InetAddress listenAddr(9450, true, false);
        dbase::net::Acceptor acceptor(listenAddr, false, false);

        acceptor.setNewConnectionCallback(
                [](dbase::net::Socket conn, const dbase::net::InetAddress& peerAddr)
                {
                    DBASE_LOG_INFO("new connection from {}", peerAddr.toIpPort());

                    const auto localAddr = conn.localAddress();
                    DBASE_LOG_INFO("local={}", localAddr.toIpPort());

                    conn.shutdownWrite();
                });

        acceptor.listen();
        DBASE_LOG_INFO("acceptor listening on {}", acceptor.listenAddress().toIpPort());

        for (int i = 0; i < 200; ++i)
        {
            acceptor.acceptAvailable(16);
            dbase::thread::current_thread::sleepForMs(50);
        }

        DBASE_LOG_INFO("acceptor example exit");
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

// powershell: Test-NetConnection 127.0.0.1 -Port 9450

/*
PS C:\Users\wangtao.deng> Test-NetConnection 127.0.0.1 -Port 9450


ComputerName     : 127.0.0.1
RemoteAddress    : 127.0.0.1
RemotePort       : 9450
InterfaceAlias   : Loopback Pseudo-Interface 1
SourceAddress    : 127.0.0.1
TcpTestSucceeded : True
*/