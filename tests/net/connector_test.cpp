#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>

#include "dbase/net/connector.h"
#include "dbase/net/event_loop.h"
#include "dbase/net/inet_address.h"
#include "dbase/net/socket.h"
#include "dbase/net/socket_ops.h"

namespace
{
using namespace std::chrono_literals;
using dbase::net::Connector;
using dbase::net::EventLoop;
using dbase::net::InetAddress;
using dbase::net::kInvalidSocket;
using dbase::net::Socket;
using dbase::net::SocketOps;
using dbase::net::SocketType;

struct SocketOpsInit
{
        SocketOpsInit()
        {
            static_cast<void>(SocketOps::initialize());
        }
};

const SocketOpsInit kSocketOpsInit{};

class LoopThread
{
    public:
        LoopThread()
        {
            std::promise<EventLoop*> ready;
            m_ready = ready.get_future();

            m_thread = std::thread([p = std::move(ready)]() mutable
                                   {
                                       EventLoop loop;
                                       p.set_value(&loop);
                                       loop.loop(); });
        }

        ~LoopThread()
        {
            if (m_loop != nullptr)
            {
                m_loop->quit();
            }
            if (m_thread.joinable())
            {
                m_thread.join();
            }
        }

        EventLoop* loop()
        {
            if (m_loop == nullptr)
            {
                m_loop = m_ready.get();
            }
            return m_loop;
        }

    private:
        std::future<EventLoop*> m_ready;
        EventLoop* m_loop{nullptr};
        std::thread m_thread;
};

[[nodiscard]] InetAddress loopbackAddress(std::uint16_t port = 0)
{
    return InetAddress(port, true, false);
}

[[nodiscard]] Socket makeListeningServer(InetAddress* outLocal = nullptr)
{
    Socket server = Socket::createTcp(AF_INET);
    server.setReuseAddr(true);
    server.bindAddress(loopbackAddress(0));
    server.listen();

    if (outLocal != nullptr)
    {
        *outLocal = server.localAddress();
    }
    return server;
}

[[nodiscard]] bool waitUntil(const std::function<bool()>& predicate, std::chrono::milliseconds timeout)
{
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline)
    {
        if (predicate())
        {
            return true;
        }
        std::this_thread::sleep_for(10ms);
    }
    return predicate();
}
}  // namespace

TEST_CASE("Connector constructor rejects null loop", "[net][connector]")
{
    REQUIRE_THROWS_AS(
            std::make_shared<Connector>(nullptr, loopbackAddress(12345)),
            std::invalid_argument);
}

TEST_CASE("Connector default state after construction", "[net][connector]")
{
    LoopThread loopThread;
    EventLoop* loop = loopThread.loop();

    auto connector = std::make_shared<Connector>(loop, loopbackAddress(12345));

    REQUIRE(connector->ownerLoop() == loop);
    REQUIRE(connector->serverAddress().toIp() == "127.0.0.1");
    REQUIRE(connector->serverAddress().port() == 12345);
    REQUIRE(connector->state() == Connector::State::Disconnected);
    REQUIRE_FALSE(connector->started());
    REQUIRE(connector->initialRetryDelayMs() == 500);
    REQUIRE(connector->maxRetryDelayMs() == 30000);
    REQUIRE(connector->currentRetryDelayMs() == 500);
}

TEST_CASE("Connector setRetryDelayMs normalizes invalid values", "[net][connector]")
{
    LoopThread loopThread;
    EventLoop* loop = loopThread.loop();

    auto connector = std::make_shared<Connector>(loop, loopbackAddress(65001));

    connector->setRetryDelayMs(-10, -1);
    REQUIRE(connector->initialRetryDelayMs() == 1);
    REQUIRE(connector->maxRetryDelayMs() == 1);
    REQUIRE(connector->currentRetryDelayMs() == 1);

    connector->setRetryDelayMs(100, 50);
    REQUIRE(connector->initialRetryDelayMs() == 100);
    REQUIRE(connector->maxRetryDelayMs() == 100);
    REQUIRE(connector->currentRetryDelayMs() == 100);

    connector->setRetryDelayMs(20, 80);
    REQUIRE(connector->initialRetryDelayMs() == 20);
    REQUIRE(connector->maxRetryDelayMs() == 80);
    REQUIRE(connector->currentRetryDelayMs() == 20);
}

TEST_CASE("Connector start and stop update started flag", "[net][connector]")
{
    LoopThread loopThread;
    EventLoop* loop = loopThread.loop();

    auto connector = std::make_shared<Connector>(loop, loopbackAddress(54321));

    REQUIRE_FALSE(connector->started());

    connector->start();
    REQUIRE(connector->started());

    connector->stop();
    REQUIRE_FALSE(connector->started());
}

TEST_CASE("Connector connects successfully and invokes callback", "[net][connector]")
{
    InetAddress serverAddr;
    Socket server = makeListeningServer(&serverAddr);

    LoopThread loopThread;
    EventLoop* loop = loopThread.loop();

    auto connector = std::make_shared<Connector>(loop, serverAddr);

    std::promise<void> connectedPromise;
    auto connectedFuture = connectedPromise.get_future();

    std::atomic<int> callbackCount{0};
    std::atomic<SocketType> connectedFd{kInvalidSocket};

    connector->setNewConnectionCallback(
            [&callbackCount, &connectedFd, &connectedPromise](Socket socket)
            {
                callbackCount.fetch_add(1, std::memory_order_relaxed);
                connectedFd.store(socket.fd(), std::memory_order_release);
                connectedPromise.set_value();
            });

    connector->start();

    REQUIRE(connectedFuture.wait_for(1000ms) == std::future_status::ready);
    REQUIRE(callbackCount.load(std::memory_order_relaxed) == 1);
    REQUIRE(connector->started());
    REQUIRE(connector->state() == Connector::State::Connected);
    REQUIRE(connectedFd.load(std::memory_order_acquire) != kInvalidSocket);

    InetAddress peerAddr;
    Socket accepted;
    REQUIRE(waitUntil(
            [&]()
            {
                try
                {
                    accepted = server.accept(&peerAddr);
                    return accepted.valid();
                }
                catch (...)
                {
                    return false;
                }
            },
            1000ms));

    REQUIRE(accepted.valid());
    REQUIRE(peerAddr.isIpv4());
    REQUIRE(peerAddr.isLoopbackIp());
}

TEST_CASE("Connector successful connection resets retry delay to initial value", "[net][connector]")
{
    InetAddress serverAddr;
    Socket server = makeListeningServer(&serverAddr);

    LoopThread loopThread;
    EventLoop* loop = loopThread.loop();

    auto connector = std::make_shared<Connector>(loop, serverAddr);
    connector->setRetryDelayMs(20, 80);

    std::promise<void> done;
    auto future = done.get_future();

    connector->setNewConnectionCallback(
            [&done](Socket)
            {
                done.set_value();
            });

    connector->start();

    REQUIRE(future.wait_for(1000ms) == std::future_status::ready);
    REQUIRE(connector->state() == Connector::State::Connected);
    REQUIRE(connector->currentRetryDelayMs() == connector->initialRetryDelayMs());
}

TEST_CASE("Connector can connect successfully without callback", "[net][connector]")
{
    InetAddress serverAddr;
    Socket server = makeListeningServer(&serverAddr);

    LoopThread loopThread;
    EventLoop* loop = loopThread.loop();

    auto connector = std::make_shared<Connector>(loop, serverAddr);
    connector->start();

    REQUIRE(waitUntil(
            [&]()
            {
                return connector->state() == Connector::State::Connected;
            },
            1000ms));

    REQUIRE(connector->started());
    REQUIRE(connector->state() == Connector::State::Connected);

    InetAddress peerAddr;
    Socket accepted;
    REQUIRE(waitUntil(
            [&]()
            {
                try
                {
                    accepted = server.accept(&peerAddr);
                    return accepted.valid();
                }
                catch (...)
                {
                    return false;
                }
            },
            1000ms));

    REQUIRE(accepted.valid());
}

TEST_CASE("Connector failed connect does not invoke success callback", "[net][connector]")
{
    LoopThread loopThread;
    EventLoop* loop = loopThread.loop();

    const InetAddress unreachableAddr(65001, true, false);
    auto connector = std::make_shared<Connector>(loop, unreachableAddr);
    connector->setRetryDelayMs(20, 80);

    std::atomic<int> callbackCount{0};
    connector->setNewConnectionCallback(
            [&callbackCount](Socket)
            {
                callbackCount.fetch_add(1, std::memory_order_relaxed);
            });

    connector->start();

    std::this_thread::sleep_for(300ms);

    REQUIRE(connector->started());
    REQUIRE(callbackCount.load(std::memory_order_relaxed) == 0);
    REQUIRE(connector->state() != Connector::State::Connected);
}

TEST_CASE("Connector stop clears started flag after failed connect attempt", "[net][connector]")
{
    LoopThread loopThread;
    EventLoop* loop = loopThread.loop();

    const InetAddress unreachableAddr(65002, true, false);
    auto connector = std::make_shared<Connector>(loop, unreachableAddr);
    connector->setRetryDelayMs(20, 80);

    std::atomic<int> callbackCount{0};
    connector->setNewConnectionCallback(
            [&callbackCount](Socket)
            {
                callbackCount.fetch_add(1, std::memory_order_relaxed);
            });

    connector->start();

    std::this_thread::sleep_for(200ms);
    connector->stop();

    REQUIRE_FALSE(connector->started());
    REQUIRE(callbackCount.load(std::memory_order_relaxed) == 0);

    std::this_thread::sleep_for(200ms);

    REQUIRE_FALSE(connector->started());
    REQUIRE(connector->state() != Connector::State::Connected);
    REQUIRE(callbackCount.load(std::memory_order_relaxed) == 0);
}

TEST_CASE("Connector can stop after successful connection", "[net][connector]")
{
    InetAddress serverAddr;
    Socket server = makeListeningServer(&serverAddr);

    LoopThread loopThread;
    EventLoop* loop = loopThread.loop();

    auto connector = std::make_shared<Connector>(loop, serverAddr);

    std::promise<void> done;
    auto future = done.get_future();

    connector->setNewConnectionCallback(
            [&done](Socket)
            {
                done.set_value();
            });

    connector->start();

    REQUIRE(future.wait_for(1000ms) == std::future_status::ready);
    REQUIRE(connector->state() == Connector::State::Connected);

    connector->stop();

    REQUIRE_FALSE(connector->started());
}

TEST_CASE("Connector restart sets started flag again after stop", "[net][connector]")
{
    InetAddress serverAddr;
    Socket server = makeListeningServer(&serverAddr);

    LoopThread loopThread;
    EventLoop* loop = loopThread.loop();

    auto connector = std::make_shared<Connector>(loop, serverAddr);

    std::promise<void> firstDone;
    auto firstFuture = firstDone.get_future();

    connector->setNewConnectionCallback(
            [&firstDone](Socket)
            {
                firstDone.set_value();
            });

    connector->start();

    REQUIRE(firstFuture.wait_for(1000ms) == std::future_status::ready);

    connector->stop();
    REQUIRE_FALSE(connector->started());

    connector->restart();

    REQUIRE(waitUntil(
            [&]()
            {
                return connector->started();
            },
            500ms));
}