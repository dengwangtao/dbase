#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <future>
#include <stdexcept>
#include <thread>

#include "dbase/net/event_loop.h"

namespace
{
using namespace std::chrono_literals;
using dbase::net::EventLoop;

struct SocketOpsInit
{
        SocketOpsInit()
        {
            dbase::net::SocketOps::initialize();
        }
};

const SocketOpsInit kSocketOpsInit{};

TEST_CASE("EventLoop default state", "[net][event_loop][timer]")
{
    EventLoop loop;

    REQUIRE_FALSE(loop.looping());
    REQUIRE_FALSE(loop.quitRequested());
    REQUIRE(loop.isInLoopThread());
    REQUIRE_NOTHROW(loop.assertInLoopThread());
    REQUIRE(loop.threadId() != 0);
    REQUIRE(loop.poller() != nullptr);
    REQUIRE(loop.activeChannels().empty());
}

TEST_CASE("EventLoop loop can run and quit in owner thread", "[net][event_loop][timer]")
{
    EventLoop loop;

    REQUIRE_FALSE(loop.looping());
    REQUIRE_FALSE(loop.quitRequested());

    loop.runAfter(10ms, [&loop]()
                  { loop.quit(); });

    loop.loop();

    REQUIRE_FALSE(loop.looping());
    REQUIRE(loop.quitRequested());
}

TEST_CASE("EventLoop loop from non owner thread throws", "[net][event_loop][timer]")
{
    EventLoop loop;

    std::promise<void> ready;
    auto readyFuture = ready.get_future();

    std::thread other([&loop, &ready]()
                      {
                          ready.set_value();
                          REQUIRE_THROWS_AS(loop.loop(), std::runtime_error); });

    REQUIRE(readyFuture.wait_for(200ms) == std::future_status::ready);
    other.join();

    REQUIRE_FALSE(loop.looping());
}

TEST_CASE("EventLoop runAfter executes one shot timer", "[net][event_loop][timer]")
{
    EventLoop loop;

    std::atomic<int> count{0};

    loop.runAfter(20ms, [&]()
                  {
                      count.fetch_add(1, std::memory_order_relaxed);
                      loop.quit(); });

    loop.loop();

    REQUIRE(count.load(std::memory_order_relaxed) == 1);
}

TEST_CASE("EventLoop runAfter with negative delay executes promptly", "[net][event_loop][timer]")
{
    EventLoop loop;

    std::atomic<int> count{0};

    loop.runAfter(-10ms, [&]()
                  {
                      count.fetch_add(1, std::memory_order_relaxed);
                      loop.quit(); });

    loop.loop();

    REQUIRE(count.load(std::memory_order_relaxed) == 1);
}

TEST_CASE("EventLoop runEvery rejects non positive interval", "[net][event_loop][timer]")
{
    EventLoop loop;

    REQUIRE_THROWS_AS(loop.runEvery(0ms, []() {}), std::invalid_argument);
    REQUIRE_THROWS_AS(loop.runEvery(-1ms, []() {}), std::invalid_argument);
}

TEST_CASE("EventLoop rejects empty timer callback", "[net][event_loop][timer]")
{
    EventLoop loop;

    REQUIRE_THROWS_AS(loop.runAfter(10ms, {}), std::invalid_argument);
    REQUIRE_THROWS_AS(loop.runEvery(10ms, {}), std::invalid_argument);
}

TEST_CASE("EventLoop runEvery repeats until cancelled", "[net][event_loop][timer]")
{
    EventLoop loop;

    std::atomic<int> count{0};
    EventLoop::TimerId repeatingId = 0;

    repeatingId = loop.runEvery(20ms, [&]()
                                {
                                    const int current = count.fetch_add(1, std::memory_order_relaxed) + 1;
                                    if (current >= 3)
                                    {
                                        loop.cancelTimer(repeatingId);
                                        loop.runAfter(80ms, [&loop]()
                                                      { loop.quit(); });
                                    } });

    loop.loop();

    REQUIRE(count.load(std::memory_order_relaxed) == 3);
}

TEST_CASE("EventLoop cancelTimer prevents one shot timer execution", "[net][event_loop][timer]")
{
    EventLoop loop;

    std::atomic<int> count{0};

    const auto id = loop.runAfter(120ms, [&]()
                                  { count.fetch_add(1, std::memory_order_relaxed); });

    loop.cancelTimer(id);
    loop.runAfter(180ms, [&loop]()
                  { loop.quit(); });

    loop.loop();

    REQUIRE(count.load(std::memory_order_relaxed) == 0);
}

TEST_CASE("EventLoop cancelTimer from another thread prevents execution", "[net][event_loop][timer]")
{
    EventLoop loop;

    std::atomic<int> count{0};
    std::promise<void> timerCreated;
    auto timerCreatedFuture = timerCreated.get_future();

    std::thread canceller([&]()
                          {
                              REQUIRE(timerCreatedFuture.wait_for(500ms) == std::future_status::ready);
                              std::this_thread::sleep_for(20ms);
                              loop.cancelTimer(1); });

    const auto id = loop.runAfter(150ms, [&]()
                                  { count.fetch_add(1, std::memory_order_relaxed); });
    REQUIRE(id == 1);
    timerCreated.set_value();

    loop.runAfter(250ms, [&loop]()
                  { loop.quit(); });

    loop.loop();
    canceller.join();

    REQUIRE(count.load(std::memory_order_relaxed) == 0);
}

TEST_CASE("EventLoop cancelTimer zero is no op", "[net][event_loop][timer]")
{
    EventLoop loop;

    std::atomic<int> count{0};

    loop.cancelTimer(0);

    loop.runAfter(20ms, [&]()
                  {
                      count.fetch_add(1, std::memory_order_relaxed);
                      loop.quit(); });

    loop.loop();

    REQUIRE(count.load(std::memory_order_relaxed) == 1);
}

TEST_CASE("EventLoop multiple one shot timers all execute", "[net][event_loop][timer]")
{
    EventLoop loop;

    constexpr int taskCount = 5;
    std::atomic<int> count{0};

    for (int i = 0; i < taskCount; ++i)
    {
        loop.runAfter(10ms + std::chrono::milliseconds(i * 10), [&]()
                      { count.fetch_add(1, std::memory_order_relaxed); });
    }

    loop.runAfter(120ms, [&loop]()
                  { loop.quit(); });

    loop.loop();

    REQUIRE(count.load(std::memory_order_relaxed) == taskCount);
}

TEST_CASE("EventLoop timer ids are unique and increasing on same thread", "[net][event_loop][timer]")
{
    EventLoop loop;

    const auto id1 = loop.runAfter(100ms, []() {});
    const auto id2 = loop.runAfter(120ms, []() {});
    const auto id3 = loop.runEvery(140ms, []() {});

    REQUIRE(id1 < id2);
    REQUIRE(id2 < id3);
}

TEST_CASE("EventLoop runInLoop executes immediately on owner thread", "[net][event_loop][timer]")
{
    EventLoop loop;

    std::atomic<int> count{0};

    loop.runInLoop([&]()
                   { count.fetch_add(1, std::memory_order_relaxed); });

    REQUIRE(count.load(std::memory_order_relaxed) == 1);
}

TEST_CASE("EventLoop queueInLoop from another thread runs inside loop", "[net][event_loop][timer]")
{
    EventLoop loop;

    std::atomic<int> count{0};

    std::thread producer([&loop, &count]()
                         {
                             std::this_thread::sleep_for(20ms);
                             loop.queueInLoop([&]()
                                              { count.fetch_add(1, std::memory_order_relaxed); });
                             loop.queueInLoop([&loop]()
                                              { loop.quit(); }); });

    loop.loop();
    producer.join();

    REQUIRE(count.load(std::memory_order_relaxed) == 1);
}

TEST_CASE("EventLoop runInLoop from another thread is deferred into loop", "[net][event_loop][timer]")
{
    EventLoop loop;

    std::atomic<int> count{0};

    std::thread producer([&loop, &count]()
                         {
                             std::this_thread::sleep_for(20ms);
                             loop.runInLoop([&]()
                                            { count.fetch_add(1, std::memory_order_relaxed); });
                             loop.runInLoop([&loop]()
                                            { loop.quit(); }); });

    loop.loop();
    producer.join();

    REQUIRE(count.load(std::memory_order_relaxed) == 1);
}

TEST_CASE("EventLoop repeating timer can cancel itself", "[net][event_loop][timer]")
{
    EventLoop loop;

    std::atomic<int> count{0};
    std::atomic<EventLoop::TimerId> timerId{0};

    const auto id = loop.runEvery(20ms, [&]()
                                  {
                                      const int current = count.fetch_add(1, std::memory_order_relaxed) + 1;
                                      if (current == 3)
                                      {
                                          loop.cancelTimer(timerId.load(std::memory_order_acquire));
                                          loop.runAfter(80ms, [&loop]()
                                                        { loop.quit(); });
                                      } });

    timerId.store(id, std::memory_order_release);

    loop.loop();

    REQUIRE(count.load(std::memory_order_relaxed) == 3);
}

TEST_CASE("EventLoop cancelled repeating timer stops firing", "[net][event_loop][timer]")
{
    EventLoop loop;

    std::atomic<int> count{0};
    EventLoop::TimerId repeatingId = 0;

    repeatingId = loop.runEvery(20ms, [&]()
                                {
                                    const int current = count.fetch_add(1, std::memory_order_relaxed) + 1;
                                    if (current == 2)
                                    {
                                        loop.cancelTimer(repeatingId);
                                    } });

    loop.runAfter(140ms, [&loop]()
                  { loop.quit(); });

    loop.loop();

    REQUIRE(count.load(std::memory_order_relaxed) == 2);
}

TEST_CASE("EventLoop quit from another thread wakes loop", "[net][event_loop][timer]")
{
    EventLoop loop;

    std::thread quitter([&loop]()
                        {
                            std::this_thread::sleep_for(30ms);
                            loop.quit(); });

    loop.loop();
    quitter.join();

    REQUIRE(loop.quitRequested());
    REQUIRE_FALSE(loop.looping());
}

TEST_CASE("EventLoop activeChannels is empty in pure timer workflow", "[net][event_loop][timer]")
{
    EventLoop loop;

    loop.runAfter(20ms, [&loop]()
                  { loop.quit(); });

    loop.loop();

    REQUIRE(loop.activeChannels().empty());
}

TEST_CASE("EventLoop can process queued functor before delayed quit timer", "[net][event_loop][timer]")
{
    EventLoop loop;

    std::atomic<int> count{0};

    std::thread producer([&loop, &count]()
                         {
                             std::this_thread::sleep_for(10ms);
                             loop.queueInLoop([&]()
                                              { count.fetch_add(1, std::memory_order_relaxed); }); });

    loop.runAfter(80ms, [&loop]()
                  { loop.quit(); });

    loop.loop();
    producer.join();

    REQUIRE(count.load(std::memory_order_relaxed) == 1);
}
}  // namespace