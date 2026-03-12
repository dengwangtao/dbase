#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <future>
#include <stdexcept>
#include <thread>
#include <vector>

#include "dbase/thread/thread_pool.h"
#include "dbase/thread/timer_queue.h"

namespace
{
using namespace std::chrono_literals;
using dbase::thread::ThreadPool;
using dbase::thread::TimerQueue;

TEST_CASE("TimerQueue default state before start", "[thread][timer_queue]")
{
    TimerQueue queue;

    REQUIRE_FALSE(queue.started());
    REQUIRE(queue.stopped());
    REQUIRE(queue.size() == 0);
}

TEST_CASE("TimerQueue start updates state", "[thread][timer_queue]")
{
    TimerQueue queue;

    queue.start();

    REQUIRE(queue.started());
    REQUIRE_FALSE(queue.stopped());
    REQUIRE(queue.size() == 0);

    queue.stop();

    REQUIRE_FALSE(queue.started());
    REQUIRE(queue.stopped());
    REQUIRE(queue.size() == 0);
}

TEST_CASE("TimerQueue start twice throws", "[thread][timer_queue]")
{
    TimerQueue queue;

    queue.start();
    REQUIRE_THROWS_AS(queue.start(), std::logic_error);

    queue.stop();
}

TEST_CASE("TimerQueue stop is idempotent", "[thread][timer_queue]")
{
    TimerQueue queue;

    queue.start();
    queue.stop();

    REQUIRE(queue.stopped());
    REQUIRE_NOTHROW(queue.stop());
    REQUIRE(queue.stopped());
}

TEST_CASE("TimerQueue scheduling before start throws", "[thread][timer_queue]")
{
    TimerQueue queue;

    REQUIRE_THROWS_AS(
            queue.runAfter(10ms, []() {}),
            std::logic_error);

    REQUIRE_THROWS_AS(
            queue.runAt(TimerQueue::Clock::now() + 10ms, []() {}),
            std::logic_error);

    REQUIRE_THROWS_AS(
            queue.runEvery(10ms, []() {}),
            std::logic_error);
}

TEST_CASE("TimerQueue empty task is rejected", "[thread][timer_queue]")
{
    TimerQueue queue;
    queue.start();

    REQUIRE_THROWS_AS(queue.runAfter(10ms, {}), std::invalid_argument);
    REQUIRE_THROWS_AS(queue.runAt(TimerQueue::Clock::now() + 10ms, {}), std::invalid_argument);
    REQUIRE_THROWS_AS(queue.runEvery(10ms, {}), std::invalid_argument);

    queue.stop();
}

TEST_CASE("TimerQueue runEvery rejects non-positive interval", "[thread][timer_queue]")
{
    TimerQueue queue;
    queue.start();

    REQUIRE_THROWS_AS(queue.runEvery(0ms, []() {}), std::invalid_argument);
    REQUIRE_THROWS_AS(queue.runEvery(-1ms, []() {}), std::invalid_argument);

    queue.stop();
}

TEST_CASE("TimerQueue runAfter executes one-shot task", "[thread][timer_queue]")
{
    TimerQueue queue;
    queue.start();

    std::promise<void> done;
    auto future = done.get_future();
    std::atomic<int> count{0};

    const auto id = queue.runAfter(20ms, [&]()
                                   {
                                       count.fetch_add(1, std::memory_order_relaxed);
                                       done.set_value(); });

    REQUIRE(id != 0);

    REQUIRE(future.wait_for(500ms) == std::future_status::ready);
    REQUIRE(count.load(std::memory_order_relaxed) == 1);

    std::this_thread::sleep_for(30ms);
    REQUIRE(queue.size() == 0);

    queue.stop();
}

TEST_CASE("TimerQueue runAt executes one-shot task", "[thread][timer_queue]")
{
    TimerQueue queue;
    queue.start();

    std::promise<void> done;
    auto future = done.get_future();
    std::atomic<int> count{0};

    const auto id = queue.runAt(TimerQueue::Clock::now() + 20ms, [&]()
                                {
                                    count.fetch_add(1, std::memory_order_relaxed);
                                    done.set_value(); });

    REQUIRE(id != 0);
    REQUIRE(future.wait_for(500ms) == std::future_status::ready);
    REQUIRE(count.load(std::memory_order_relaxed) == 1);

    queue.stop();
}

TEST_CASE("TimerQueue runAt in the past executes promptly", "[thread][timer_queue]")
{
    TimerQueue queue;
    queue.start();

    std::promise<void> done;
    auto future = done.get_future();

    const auto id = queue.runAt(TimerQueue::Clock::now() - 10ms, [&]()
                                { done.set_value(); });

    REQUIRE(id != 0);
    REQUIRE(future.wait_for(300ms) == std::future_status::ready);

    queue.stop();
}

TEST_CASE("TimerQueue runEvery repeats until cancelled", "[thread][timer_queue]")
{
    TimerQueue queue;
    queue.start();

    std::atomic<int> count{0};
    std::promise<void> reached;
    auto future = reached.get_future();
    std::atomic<bool> notified{false};

    TimerQueue::TimerId id = 0;
    id = queue.runEvery(20ms, [&]()
                        {
                            const int current = count.fetch_add(1, std::memory_order_relaxed) + 1;
                            if (current >= 3 && !notified.exchange(true, std::memory_order_acq_rel))
                            {
                                reached.set_value();
                            } });

    REQUIRE(id != 0);
    REQUIRE(future.wait_for(700ms) == std::future_status::ready);

    const int beforeCancel = count.load(std::memory_order_relaxed);
    REQUIRE(beforeCancel >= 3);

    REQUIRE(queue.cancel(id));

    std::this_thread::sleep_for(120ms);

    const int afterCancel = count.load(std::memory_order_relaxed);
    REQUIRE(afterCancel == beforeCancel);

    queue.stop();
}

TEST_CASE("TimerQueue cancel prevents one-shot task from running", "[thread][timer_queue]")
{
    TimerQueue queue;
    queue.start();

    std::atomic<int> count{0};

    const auto id = queue.runAfter(120ms, [&]()
                                   { count.fetch_add(1, std::memory_order_relaxed); });

    REQUIRE(id != 0);
    REQUIRE(queue.cancel(id));

    std::this_thread::sleep_for(220ms);

    REQUIRE(count.load(std::memory_order_relaxed) == 0);

    queue.stop();
}

TEST_CASE("TimerQueue cancel returns false when same timer is cancelled twice", "[thread][timer_queue]")
{
    TimerQueue queue;
    queue.start();

    const auto id = queue.runAfter(200ms, []() {});

    REQUIRE(queue.cancel(id));
    REQUIRE_FALSE(queue.cancel(id));

    queue.stop();
}

TEST_CASE("TimerQueue cancel on unknown timer id returns true first time and false second time", "[thread][timer_queue]")
{
    TimerQueue queue;
    queue.start();

    REQUIRE(queue.cancel(123456789));
    REQUIRE_FALSE(queue.cancel(123456789));

    queue.stop();
}

TEST_CASE("TimerQueue cancelAll prevents queued tasks from running", "[thread][timer_queue]")
{
    TimerQueue queue;
    queue.start();

    std::atomic<int> count{0};

    queue.runAfter(150ms, [&]()
                   { count.fetch_add(1, std::memory_order_relaxed); });
    queue.runAfter(170ms, [&]()
                   { count.fetch_add(1, std::memory_order_relaxed); });
    queue.runAfter(190ms, [&]()
                   { count.fetch_add(1, std::memory_order_relaxed); });

    REQUIRE(queue.size() == 3);

    queue.cancelAll();

    std::this_thread::sleep_for(300ms);

    REQUIRE(count.load(std::memory_order_relaxed) == 0);

    queue.stop();
}

TEST_CASE("TimerQueue size reflects queued tasks before execution", "[thread][timer_queue]")
{
    TimerQueue queue;
    queue.start();

    queue.runAfter(200ms, []() {});
    queue.runAfter(220ms, []() {});
    queue.runAfter(240ms, []() {});

    REQUIRE(queue.size() == 3);

    queue.stop();
}

TEST_CASE("TimerQueue stop clears pending tasks", "[thread][timer_queue]")
{
    TimerQueue queue;
    queue.start();

    queue.runAfter(200ms, []() {});
    queue.runAfter(220ms, []() {});
    REQUIRE(queue.size() == 2);

    queue.stop();

    REQUIRE(queue.stopped());
    REQUIRE(queue.size() == 0);
}

TEST_CASE("TimerQueue can dispatch tasks into ThreadPool", "[thread][timer_queue]")
{
    ThreadPool pool(2, "worker", 16);
    pool.start();

    TimerQueue queue(&pool);
    queue.start();

    std::promise<std::thread::id> done;
    auto future = done.get_future();
    const auto callerId = std::this_thread::get_id();

    queue.runAfter(20ms, [&done]()
                   { done.set_value(std::this_thread::get_id()); });

    REQUIRE(future.wait_for(500ms) == std::future_status::ready);
    const auto workerId = future.get();

    REQUIRE(workerId != callerId);

    queue.stop();
    pool.stop();
}

TEST_CASE("TimerQueue setThreadPool can switch dispatch target", "[thread][timer_queue]")
{
    ThreadPool pool(1, "worker", 8);
    pool.start();

    TimerQueue queue;
    queue.start();

    queue.setThreadPool(&pool);

    std::promise<std::thread::id> done;
    auto future = done.get_future();
    const auto callerId = std::this_thread::get_id();

    queue.runAfter(20ms, [&done]()
                   { done.set_value(std::this_thread::get_id()); });

    REQUIRE(future.wait_for(500ms) == std::future_status::ready);
    REQUIRE(future.get() != callerId);

    queue.stop();
    pool.stop();
}

TEST_CASE("TimerQueue generated timer ids are nonzero and increasing", "[thread][timer_queue]")
{
    TimerQueue queue;
    queue.start();

    const auto id1 = queue.runAfter(200ms, []() {});
    const auto id2 = queue.runAfter(220ms, []() {});
    const auto id3 = queue.runEvery(240ms, []() {});

    REQUIRE(id1 != 0);
    REQUIRE(id2 != 0);
    REQUIRE(id3 != 0);
    REQUIRE(id1 < id2);
    REQUIRE(id2 < id3);

    queue.stop();
}

TEST_CASE("TimerQueue multiple one-shot timers all execute", "[thread][timer_queue]")
{
    TimerQueue queue;
    queue.start();

    constexpr int taskCount = 5;
    std::atomic<int> count{0};
    std::promise<void> done;
    auto future = done.get_future();
    std::atomic<bool> notified{false};

    for (int i = 0; i < taskCount; ++i)
    {
        queue.runAfter(20ms + std::chrono::milliseconds(i * 10), [&]()
                       {
                           const int current = count.fetch_add(1, std::memory_order_relaxed) + 1;
                           if (current == taskCount && !notified.exchange(true, std::memory_order_acq_rel))
                           {
                               done.set_value();
                           } });
    }

    REQUIRE(future.wait_for(800ms) == std::future_status::ready);
    REQUIRE(count.load(std::memory_order_relaxed) == taskCount);

    queue.stop();
}

TEST_CASE("TimerQueue repeat task can cancel itself", "[thread][timer_queue]")
{
    TimerQueue queue;
    queue.start();

    std::atomic<int> count{0};
    std::promise<void> done;
    auto future = done.get_future();
    std::atomic<TimerQueue::TimerId> timerId{0};

    const auto id = queue.runEvery(20ms, [&]()
                                   {
                                       const int current = count.fetch_add(1, std::memory_order_relaxed) + 1;
                                       if (current == 3)
                                       {
                                           static_cast<void>(queue.cancel(timerId.load(std::memory_order_acquire)));
                                           done.set_value();
                                       } });

    timerId.store(id, std::memory_order_release);

    REQUIRE(future.wait_for(700ms) == std::future_status::ready);

    const int beforeSleep = count.load(std::memory_order_relaxed);
    std::this_thread::sleep_for(120ms);
    REQUIRE(count.load(std::memory_order_relaxed) == beforeSleep);

    queue.stop();
}

TEST_CASE("TimerQueue stop before expiration prevents pending execution", "[thread][timer_queue]")
{
    TimerQueue queue;
    queue.start();

    std::atomic<int> count{0};

    queue.runAfter(150ms, [&]()
                   { count.fetch_add(1, std::memory_order_relaxed); });

    std::this_thread::sleep_for(20ms);
    queue.stop();

    std::this_thread::sleep_for(220ms);

    REQUIRE(count.load(std::memory_order_relaxed) == 0);
}

TEST_CASE("TimerQueue cancelAll also stops repeat tasks from continuing", "[thread][timer_queue]")
{
    TimerQueue queue;
    queue.start();

    std::atomic<int> count{0};
    std::promise<void> reached;
    auto future = reached.get_future();
    std::atomic<bool> notified{false};

    queue.runEvery(20ms, [&]()
                   {
                       const int current = count.fetch_add(1, std::memory_order_relaxed) + 1;
                       if (current >= 2 && !notified.exchange(true, std::memory_order_acq_rel))
                       {
                           reached.set_value();
                       } });

    REQUIRE(future.wait_for(500ms) == std::future_status::ready);

    queue.cancelAll();
    const int beforeSleep = count.load(std::memory_order_relaxed);

    std::this_thread::sleep_for(120ms);

    REQUIRE(count.load(std::memory_order_relaxed) == beforeSleep);

    queue.stop();
}
}  // namespace