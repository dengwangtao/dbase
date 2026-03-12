#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <future>
#include <stdexcept>
#include <thread>
#include <vector>

#include "dbase/sync/count_down_latch.h"

namespace
{
using namespace std::chrono_literals;
using dbase::sync::CountDownLatch;
}  // namespace

TEST_CASE("CountDownLatch constructor rejects negative count", "[sync][count_down_latch]")
{
    REQUIRE_THROWS_AS(CountDownLatch(-1), std::invalid_argument);
}

TEST_CASE("CountDownLatch default state with zero count", "[sync][count_down_latch]")
{
    CountDownLatch latch(0);

    REQUIRE(latch.count() == 0);
    REQUIRE(latch.waitFor(0));
    REQUIRE(latch.waitFor(10));
    REQUIRE_NOTHROW(latch.wait());
}

TEST_CASE("CountDownLatch reports initial count", "[sync][count_down_latch]")
{
    CountDownLatch latch(3);

    REQUIRE(latch.count() == 3);
}

TEST_CASE("CountDownLatch countDown decrements until zero", "[sync][count_down_latch]")
{
    CountDownLatch latch(3);

    latch.countDown();
    REQUIRE(latch.count() == 2);

    latch.countDown();
    REQUIRE(latch.count() == 1);

    latch.countDown();
    REQUIRE(latch.count() == 0);
}

TEST_CASE("CountDownLatch countDown at zero is no-op", "[sync][count_down_latch]")
{
    CountDownLatch latch(0);

    latch.countDown();
    REQUIRE(latch.count() == 0);

    latch.countDown();
    REQUIRE(latch.count() == 0);
}

TEST_CASE("CountDownLatch waitFor times out while count is nonzero", "[sync][count_down_latch]")
{
    CountDownLatch latch(1);

    REQUIRE_FALSE(latch.waitFor(20));
    REQUIRE(latch.count() == 1);
}

TEST_CASE("CountDownLatch waitFor succeeds after reaching zero", "[sync][count_down_latch]")
{
    CountDownLatch latch(1);

    std::thread worker([&latch]()
                       {
                           std::this_thread::sleep_for(30ms);
                           latch.countDown(); });

    REQUIRE(latch.waitFor(200));
    REQUIRE(latch.count() == 0);

    worker.join();
}

TEST_CASE("CountDownLatch wait blocks until count reaches zero", "[sync][count_down_latch]")
{
    CountDownLatch latch(2);
    std::atomic<bool> finished{false};

    std::thread waiter([&]()
                       {
                           latch.wait();
                           finished.store(true, std::memory_order_release); });

    std::this_thread::sleep_for(20ms);
    REQUIRE_FALSE(finished.load(std::memory_order_acquire));

    latch.countDown();
    std::this_thread::sleep_for(20ms);
    REQUIRE_FALSE(finished.load(std::memory_order_acquire));

    latch.countDown();
    waiter.join();

    REQUIRE(finished.load(std::memory_order_acquire));
    REQUIRE(latch.count() == 0);
}

TEST_CASE("CountDownLatch releases multiple waiting threads", "[sync][count_down_latch]")
{
    CountDownLatch latch(1);
    std::atomic<int> released{0};
    std::vector<std::thread> waiters;

    for (int i = 0; i < 4; ++i)
    {
        waiters.emplace_back([&]()
                             {
                                 latch.wait();
                                 released.fetch_add(1, std::memory_order_relaxed); });
    }

    std::this_thread::sleep_for(20ms);
    REQUIRE(released.load(std::memory_order_relaxed) == 0);

    latch.countDown();

    for (auto& t : waiters)
    {
        t.join();
    }

    REQUIRE(released.load(std::memory_order_relaxed) == 4);
}

TEST_CASE("CountDownLatch can be used as simple startup barrier", "[sync][count_down_latch]")
{
    CountDownLatch latch(3);
    std::atomic<int> ready{0};
    std::vector<std::thread> workers;

    for (int i = 0; i < 3; ++i)
    {
        workers.emplace_back([&]()
                             {
                                 ready.fetch_add(1, std::memory_order_relaxed);
                                 latch.countDown(); });
    }

    REQUIRE(latch.waitFor(500));
    REQUIRE(ready.load(std::memory_order_relaxed) == 3);
    REQUIRE(latch.count() == 0);

    for (auto& t : workers)
    {
        t.join();
    }
}