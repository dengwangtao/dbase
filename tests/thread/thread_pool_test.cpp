#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <future>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "dbase/thread/thread_pool.h"

namespace
{
using namespace std::chrono_literals;
using dbase::thread::ThreadPool;

TEST_CASE("ThreadPool constructor rejects zero thread count", "[thread][thread_pool]")
{
    REQUIRE_THROWS_AS(ThreadPool(0), std::invalid_argument);
}

TEST_CASE("ThreadPool default state before start", "[thread][thread_pool]")
{
    ThreadPool pool(2, "worker", 16);

    REQUIRE(pool.threadCount() == 2);
    REQUIRE(pool.queueCapacity() == 16);
    REQUIRE(pool.pendingTaskCount() == 0);
    REQUIRE_FALSE(pool.started());
    REQUIRE(pool.stopped());
}

TEST_CASE("ThreadPool start updates state", "[thread][thread_pool]")
{
    ThreadPool pool(2, "worker", 8);

    pool.start();

    REQUIRE(pool.started());
    REQUIRE_FALSE(pool.stopped());
    REQUIRE(pool.threadCount() == 2);
    REQUIRE(pool.queueCapacity() == 8);
    REQUIRE(pool.pendingTaskCount() == 0);

    pool.stop();

    REQUIRE_FALSE(pool.started());
    REQUIRE(pool.stopped());
}

TEST_CASE("ThreadPool start twice throws", "[thread][thread_pool]")
{
    ThreadPool pool(1);

    pool.start();

    REQUIRE_THROWS_AS(pool.start(), std::logic_error);

    pool.stop();
}

TEST_CASE("ThreadPool submit before start throws", "[thread][thread_pool]")
{
    ThreadPool pool(1);

    REQUIRE_THROWS_AS(
            pool.submit([]()
                        { return 1; }),
            std::logic_error);
}

TEST_CASE("ThreadPool submit after stop throws because pool is no longer started", "[thread][thread_pool]")
{
    ThreadPool pool(1);

    pool.start();
    pool.stop();

    REQUIRE(pool.stopped());
    REQUIRE_FALSE(pool.started());

    REQUIRE_THROWS_AS(
            pool.submit([]()
                        { return 1; }),
            std::logic_error);
}

TEST_CASE("ThreadPool executes submitted void tasks", "[thread][thread_pool]")
{
    ThreadPool pool(2);
    pool.start();

    std::atomic<int> counter{0};

    auto f1 = pool.submit([&counter]()
                          { counter.fetch_add(1, std::memory_order_relaxed); });
    auto f2 = pool.submit([&counter]()
                          { counter.fetch_add(1, std::memory_order_relaxed); });
    auto f3 = pool.submit([&counter]()
                          { counter.fetch_add(1, std::memory_order_relaxed); });

    REQUIRE(f1.valid());
    REQUIRE(f2.valid());
    REQUIRE(f3.valid());

    f1.get();
    f2.get();
    f3.get();

    REQUIRE(counter.load(std::memory_order_relaxed) == 3);
    REQUIRE_FALSE(f1.valid());
    REQUIRE_FALSE(f2.valid());
    REQUIRE_FALSE(f3.valid());

    pool.stop();
}

TEST_CASE("ThreadPool executes submitted tasks and returns future results", "[thread][thread_pool]")
{
    ThreadPool pool(2);
    pool.start();

    auto f1 = pool.submit([]()
                          { return 7; });
    auto f2 = pool.submit([](int a, int b)
                          { return a + b; },
                          10,
                          20);
    auto f3 = pool.submit([](std::string s)
                          { return s + "-done"; },
                          std::string("work"));

    REQUIRE(f1.get() == 7);
    REQUIRE(f2.get() == 30);
    REQUIRE(f3.get() == "work-done");

    REQUIRE_FALSE(f1.valid());
    REQUIRE_FALSE(f2.valid());
    REQUIRE_FALSE(f3.valid());

    pool.stop();
}

TEST_CASE("ThreadPool propagates task exceptions through future", "[thread][thread_pool]")
{
    ThreadPool pool(1);
    pool.start();

    auto future = pool.submit([]() -> int
                              { throw std::runtime_error("task failed"); });

    REQUIRE_THROWS_AS(future.get(), std::runtime_error);
    REQUIRE_FALSE(future.valid());

    pool.stop();
}

TEST_CASE("ThreadPool can run many tasks and preserve total count", "[thread][thread_pool]")
{
    ThreadPool pool(4);
    pool.start();

    constexpr int taskCount = 200;
    std::atomic<int> counter{0};
    std::vector<std::future<void>> futures;
    futures.reserve(taskCount);

    for (int i = 0; i < taskCount; ++i)
    {
        futures.emplace_back(pool.submit([&counter]()
                                         { counter.fetch_add(1, std::memory_order_relaxed); }));
    }

    for (auto& future : futures)
    {
        future.get();
    }

    REQUIRE(counter.load(std::memory_order_relaxed) == taskCount);
    REQUIRE(pool.pendingTaskCount() == 0);

    pool.stop();
}

TEST_CASE("ThreadPool supports task argument forwarding", "[thread][thread_pool]")
{
    ThreadPool pool(2);
    pool.start();

    std::string text = "hello";

    auto future = pool.submit(
            [](std::string prefix, int value, const std::string& suffix)
            {
                return prefix + "-" + std::to_string(value) + "-" + suffix;
            },
            text,
            42,
            std::string("world"));

    REQUIRE(future.get() == "hello-42-world");
    REQUIRE_FALSE(future.valid());

    pool.stop();
}

TEST_CASE("ThreadPool pendingTaskCount becomes nonzero when tasks are blocked in queue", "[thread][thread_pool]")
{
    ThreadPool pool(1, "worker", 8);
    pool.start();

    std::promise<void> gate;
    auto gateFuture = gate.get_future().share();

    auto blocker = pool.submit([gateFuture]()
                               { gateFuture.wait(); });

    std::this_thread::sleep_for(20ms);

    std::vector<std::future<int>> futures;
    futures.reserve(4);

    for (int i = 0; i < 4; ++i)
    {
        futures.emplace_back(pool.submit([i]()
                                         { return i; }));
    }

    std::this_thread::sleep_for(20ms);

    REQUIRE(pool.pendingTaskCount() >= 1);

    gate.set_value();

    blocker.get();

    int sum = 0;
    for (auto& future : futures)
    {
        sum += future.get();
    }

    REQUIRE(sum == 0 + 1 + 2 + 3);
    REQUIRE(pool.pendingTaskCount() == 0);

    pool.stop();
}

TEST_CASE("ThreadPool bounded queue can accept work up to capacity plus active workers", "[thread][thread_pool]")
{
    ThreadPool pool(1, "worker", 2);
    pool.start();

    std::promise<void> gate;
    auto gateFuture = gate.get_future().share();

    auto running = pool.submit([gateFuture]()
                               { gateFuture.wait(); });

    std::this_thread::sleep_for(20ms);

    auto queued1 = pool.submit([]()
                               { return 1; });
    auto queued2 = pool.submit([]()
                               { return 2; });

    std::this_thread::sleep_for(20ms);

    REQUIRE(pool.pendingTaskCount() == 2);

    gate.set_value();

    running.get();
    REQUIRE(queued1.get() == 1);
    REQUIRE(queued2.get() == 2);
    REQUIRE(pool.pendingTaskCount() == 0);

    pool.stop();
}

TEST_CASE("ThreadPool stop is idempotent", "[thread][thread_pool]")
{
    ThreadPool pool(2);
    pool.start();

    auto future = pool.submit([]()
                              { return 123; });

    REQUIRE(future.get() == 123);

    pool.stop();
    REQUIRE(pool.stopped());
    REQUIRE_FALSE(pool.started());

    REQUIRE_NOTHROW(pool.stop());
    REQUIRE(pool.stopped());
    REQUIRE_FALSE(pool.started());
}

TEST_CASE("ThreadPool cannot be restarted after stop with current implementation", "[thread][thread_pool]")
{
    ThreadPool pool(1);

    pool.start();
    pool.stop();

    REQUIRE(pool.stopped());
    REQUIRE_FALSE(pool.started());

    REQUIRE_THROWS_AS(pool.start(), std::logic_error);
}

TEST_CASE("ThreadPool runs tasks on worker threads", "[thread][thread_pool]")
{
    ThreadPool pool(2, "worker", 8);
    pool.start();

    auto future = pool.submit([]()
                              { return std::this_thread::get_id(); });

    const auto workerId = future.get();
    const auto callerId = std::this_thread::get_id();

    REQUIRE(workerId != callerId);

    pool.stop();
}

TEST_CASE("ThreadPool supports concurrent submissions from multiple producer threads", "[thread][thread_pool]")
{
    ThreadPool pool(4, "worker", 0);
    pool.start();

    constexpr int producerCount = 4;
    constexpr int tasksPerProducer = 100;
    constexpr int totalTasks = producerCount * tasksPerProducer;

    std::atomic<int> executed{0};
    std::vector<std::thread> producers;
    producers.reserve(producerCount);

    std::vector<std::future<void>> futures;
    futures.reserve(totalTasks);

    std::mutex futuresMutex;
    std::atomic<bool> startFlag{false};

    for (int p = 0; p < producerCount; ++p)
    {
        producers.emplace_back([&pool, &futures, &futuresMutex, &executed, &startFlag]()
                               {
                                   while (!startFlag.load(std::memory_order_acquire))
                                   {
                                       std::this_thread::yield();
                                   }

                                   for (int i = 0; i < tasksPerProducer; ++i)
                                   {
                                       auto future = pool.submit([&executed]()
                                                                 {
                                                                     executed.fetch_add(1, std::memory_order_relaxed);
                                                                 });

                                       std::lock_guard<std::mutex> lock(futuresMutex);
                                       futures.emplace_back(std::move(future));
                                   } });
    }

    startFlag.store(true, std::memory_order_release);

    for (auto& producer : producers)
    {
        producer.join();
    }

    REQUIRE(static_cast<int>(futures.size()) == totalTasks);

    for (auto& future : futures)
    {
        future.get();
    }

    REQUIRE(executed.load(std::memory_order_relaxed) == totalTasks);
    REQUIRE(pool.pendingTaskCount() == 0);

    pool.stop();
}

TEST_CASE("ThreadPool stop waits for running tasks to finish", "[thread][thread_pool]")
{
    ThreadPool pool(2);
    pool.start();

    std::atomic<int> counter{0};

    auto f1 = pool.submit([&counter]()
                          {
                              std::this_thread::sleep_for(50ms);
                              counter.fetch_add(1, std::memory_order_relaxed); });
    auto f2 = pool.submit([&counter]()
                          {
                              std::this_thread::sleep_for(50ms);
                              counter.fetch_add(1, std::memory_order_relaxed); });

    pool.stop();

    REQUIRE(counter.load(std::memory_order_relaxed) == 2);
    REQUIRE(f1.valid());
    REQUIRE(f2.valid());
    REQUIRE_NOTHROW(f1.get());
    REQUIRE_NOTHROW(f2.get());
    REQUIRE_FALSE(f1.valid());
    REQUIRE_FALSE(f2.valid());
}

TEST_CASE("ThreadPool stop drains queued tasks before returning", "[thread][thread_pool]")
{
    ThreadPool pool(1, "worker", 8);
    pool.start();

    std::promise<void> gate;
    auto gateFuture = gate.get_future().share();

    std::atomic<int> counter{0};

    auto running = pool.submit([gateFuture]()
                               { gateFuture.wait(); });

    auto queued1 = pool.submit([&counter]()
                               { counter.fetch_add(1, std::memory_order_relaxed); });

    auto queued2 = pool.submit([&counter]()
                               { counter.fetch_add(1, std::memory_order_relaxed); });

    std::this_thread::sleep_for(20ms);
    REQUIRE(pool.pendingTaskCount() >= 2);

    gate.set_value();
    pool.stop();

    REQUIRE(counter.load(std::memory_order_relaxed) == 2);
    REQUIRE_NOTHROW(running.get());
    REQUIRE_NOTHROW(queued1.get());
    REQUIRE_NOTHROW(queued2.get());
    REQUIRE(pool.pendingTaskCount() == 0);
}

TEST_CASE("ThreadPool pendingTaskCount is zero after all futures are consumed", "[thread][thread_pool]")
{
    ThreadPool pool(2);
    pool.start();

    auto f1 = pool.submit([]()
                          { return 1; });
    auto f2 = pool.submit([]()
                          { return 2; });
    auto f3 = pool.submit([]()
                          { return 3; });

    REQUIRE(f1.get() == 1);
    REQUIRE(f2.get() == 2);
    REQUIRE(f3.get() == 3);

    std::this_thread::sleep_for(20ms);

    REQUIRE(pool.pendingTaskCount() == 0);

    pool.stop();
}

TEST_CASE("ThreadPool destructor gracefully stops started pool", "[thread][thread_pool]")
{
    std::atomic<int> counter{0};

    {
        ThreadPool pool(2);
        pool.start();

        auto f1 = pool.submit([&counter]()
                              {
                                  std::this_thread::sleep_for(20ms);
                                  counter.fetch_add(1, std::memory_order_relaxed); });

        auto f2 = pool.submit([&counter]()
                              {
                                  std::this_thread::sleep_for(20ms);
                                  counter.fetch_add(1, std::memory_order_relaxed); });

        REQUIRE_NOTHROW(f1.get());
        REQUIRE_NOTHROW(f2.get());
    }

    REQUIRE(counter.load(std::memory_order_relaxed) == 2);
}

TEST_CASE("ThreadPool destructor drains queued tasks on scope exit", "[thread][thread_pool]")
{
    std::atomic<int> counter{0};

    {
        ThreadPool pool(1, "worker", 8);
        pool.start();

        std::promise<void> gate;
        auto gateFuture = gate.get_future().share();

        auto blocker = pool.submit([gateFuture]()
                                   { gateFuture.wait(); });

        auto queued1 = pool.submit([&counter]()
                                   { counter.fetch_add(1, std::memory_order_relaxed); });

        auto queued2 = pool.submit([&counter]()
                                   { counter.fetch_add(1, std::memory_order_relaxed); });

        std::this_thread::sleep_for(20ms);
        REQUIRE(pool.pendingTaskCount() >= 2);

        gate.set_value();

        REQUIRE_NOTHROW(blocker.get());
        REQUIRE_NOTHROW(queued1.get());
        REQUIRE_NOTHROW(queued2.get());
    }

    REQUIRE(counter.load(std::memory_order_relaxed) == 2);
}

TEST_CASE("ThreadPool submit after stop throws", "[thread][thread_pool]")
{
    ThreadPool pool(1);
    pool.start();
    pool.stop();

    REQUIRE_THROWS_AS(
            pool.submit([]()
                        { return 1; }),
            std::logic_error);
}

}  // namespace