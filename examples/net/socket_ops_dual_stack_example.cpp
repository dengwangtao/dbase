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
        dbase::net::InetAddress ipv4Addr("127.0.0.1", 9000);
        dbase::net::InetAddress ipv6Addr("::1", 9001);

        DBASE_LOG_INFO("ipv4={} family={}", ipv4Addr.toIpPort(), static_cast<int>(ipv4Addr.addressFamily()));
        DBASE_LOG_INFO("ipv6={} family={}", ipv6Addr.toIpPort(), static_cast<int>(ipv6Addr.addressFamily()));

        dbase::net::InetAddress resolved4;
        if (dbase::net::InetAddress::resolve("localhost", &resolved4, false))
        {
            DBASE_LOG_INFO("resolved localhost preferIpv4={}", resolved4.toIp());
        }

        dbase::net::InetAddress resolved6;
        if (dbase::net::InetAddress::resolve("localhost", &resolved6, true))
        {
            DBASE_LOG_INFO("resolved localhost preferIpv6={}", resolved6.toIp());
        }

        auto sock6 = dbase::net::SocketOps::createTcpNonblockingOrDie(AF_INET6);
        auto v6onlyRet = dbase::net::SocketOps::setIpv6Only(sock6, true);
        if (!v6onlyRet)
        {
            DBASE_LOG_WARN("setIpv6Only failed: {}", v6onlyRet.error().message());
        }

        dbase::net::SocketOps::close(sock6);

        auto sock4 = dbase::net::SocketOps::createTcpNonblockingOrDie(AF_INET);
        dbase::net::SocketOps::close(sock4);
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