#include "dbase/log/log.h"
#include "dbase/net/inet_address.h"
#include "dbase/net/socket_ops.h"

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
        auto listenSock = dbase::net::SocketOps::createTcpNonblockingOrDie();

        const auto reuseAddrRet = dbase::net::SocketOps::setReuseAddr(listenSock, true);
        if (!reuseAddrRet)
        {
            DBASE_LOG_ERROR("setReuseAddr failed: {}", reuseAddrRet.error().message());
        }

        const auto noDelayRet = dbase::net::SocketOps::setTcpNoDelay(listenSock, true);
        if (!noDelayRet)
        {
            DBASE_LOG_WARN("setTcpNoDelay failed: {}", noDelayRet.error().message());
        }

        dbase::net::InetAddress listenAddr(9000, true);

        const auto bindRet = dbase::net::SocketOps::bind(listenSock, listenAddr);
        if (!bindRet)
        {
            DBASE_LOG_ERROR("bind failed: {}", bindRet.error().message());
            dbase::net::SocketOps::close(listenSock);
            dbase::net::SocketOps::cleanup();
            return 1;
        }

        const auto listenRet = dbase::net::SocketOps::listen(listenSock);
        if (!listenRet)
        {
            DBASE_LOG_ERROR("listen failed: {}", listenRet.error().message());
            dbase::net::SocketOps::close(listenSock);
            dbase::net::SocketOps::cleanup();
            return 1;
        }

        DBASE_LOG_INFO("listen on {}", listenAddr.toIpPort());

        const auto localRet = dbase::net::SocketOps::localAddress(listenSock);
        if (localRet)
        {
            DBASE_LOG_INFO("localAddress={}", localRet.value().toIpPort());
        }

        dbase::net::InetAddress resolved;
        if (dbase::net::InetAddress::resolve("localhost", &resolved))
        {
            DBASE_LOG_INFO("resolved localhost={}", resolved.toIp());
        }

        dbase::net::SocketOps::close(listenSock);
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