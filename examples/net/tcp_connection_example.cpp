#include "dbase/log/log.h"
#include "dbase/net/inet_address.h"
#include "dbase/net/socket.h"
#include "dbase/net/socket_ops.h"
#include "dbase/net/tcp_connection.h"
#include "dbase/thread/current_thread.h"

#include <memory>
#include <string_view>

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
        dbase::net::Socket listenSocket = dbase::net::Socket::createTcp(AF_INET);
        listenSocket.setReuseAddr(true);
        listenSocket.bindAddress(dbase::net::InetAddress(9560, true, false));
        listenSocket.listen();

        dbase::net::Socket clientSocket = dbase::net::Socket::createTcp(AF_INET);
        auto connectRet = dbase::net::SocketOps::connect(clientSocket.fd(), dbase::net::InetAddress("127.0.0.1", 9560));
        if (!connectRet)
        {
            DBASE_LOG_ERROR("connect failed: {}", connectRet.error().message());
            dbase::net::SocketOps::cleanup();
            return 1;
        }

        dbase::thread::current_thread::sleepForMs(100);

        dbase::net::InetAddress serverPeer;
        auto acceptRet = dbase::net::SocketOps::accept(listenSocket.fd(), &serverPeer);
        if (!acceptRet)
        {
            DBASE_LOG_ERROR("accept failed: {}", acceptRet.error().message());
            dbase::net::SocketOps::cleanup();
            return 1;
        }

        dbase::net::Socket serverSocket(acceptRet.value());

        auto serverLocal = serverSocket.localAddress();
        auto clientLocal = clientSocket.localAddress();

        auto serverConn = std::make_shared<dbase::net::TcpConnection>(
                "server-conn",
                std::move(serverSocket),
                serverLocal,
                serverPeer);

        auto clientConn = std::make_shared<dbase::net::TcpConnection>(
                "client-conn",
                std::move(clientSocket),
                clientLocal,
                dbase::net::InetAddress("127.0.0.1", 9560));

        serverConn->setMessageCallback(
                [](const dbase::net::TcpConnection::Ptr& conn, dbase::net::Buffer& buffer)
                {
                    const auto text = buffer.retrieveAllAsString();
                    DBASE_LOG_INFO("{} received: {}", conn->name(), text);
                    conn->send(std::string_view("pong"));
                    conn->flushOutput();
                });

        clientConn->setMessageCallback(
                [](const dbase::net::TcpConnection::Ptr& conn, dbase::net::Buffer& buffer)
                {
                    const auto text = buffer.retrieveAllAsString();
                    DBASE_LOG_INFO("{} received: {}", conn->name(), text);
                });

        serverConn->setCloseCallback(
                [](const dbase::net::TcpConnection::Ptr& conn)
                {
                    DBASE_LOG_INFO("{} closed", conn->name());
                });

        clientConn->setCloseCallback(
                [](const dbase::net::TcpConnection::Ptr& conn)
                {
                    DBASE_LOG_INFO("{} closed", conn->name());
                });

        serverConn->setErrorCallback(
                [](const dbase::net::TcpConnection::Ptr& conn, int err)
                {
                    DBASE_LOG_ERROR("{} error={}", conn->name(), err);
                });

        clientConn->setErrorCallback(
                [](const dbase::net::TcpConnection::Ptr& conn, int err)
                {
                    DBASE_LOG_ERROR("{} error={}", conn->name(), err);
                });

        serverConn->connectEstablished();
        clientConn->connectEstablished();

        clientConn->send(std::string_view("ping"));
        clientConn->flushOutput();

        dbase::thread::current_thread::sleepForMs(100);

        serverConn->receiveOnce();
        dbase::thread::current_thread::sleepForMs(100);
        clientConn->receiveOnce();

        clientConn->shutdown();
        clientConn->flushOutput();

        dbase::thread::current_thread::sleepForMs(100);

        serverConn->forceClose();
        clientConn->forceClose();

        serverConn->connectDestroyed();
        clientConn->connectDestroyed();

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