#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "dbase/sync/blocking_queue.h"

namespace
{
using namespace std::chrono_literals;
using dbase::sync::BlockingQueue;

TEST_CASE("BlockingQueue default state", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue;

    REQUIRE(queue.capacity() == 0);
    REQUIRE(queue.size() == 0);
    REQUIRE(queue.empty());
    REQUIRE_FALSE(queue.stopped());
}

TEST_CASE("BlockingQueue bounded queue reports configured capacity", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue(8);

    REQUIRE(queue.capacity() == 8);
    REQUIRE(queue.size() == 0);
    REQUIRE(queue.empty());
    REQUIRE_FALSE(queue.stopped());
}

TEST_CASE("BlockingQueue push and pop preserve FIFO order", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue;

    queue.push(1);
    queue.push(2);
    queue.push(3);

    REQUIRE(queue.size() == 3);
    REQUIRE_FALSE(queue.empty());

    REQUIRE(queue.pop() == 1);
    REQUIRE(queue.pop() == 2);
    REQUIRE(queue.pop() == 3);

    REQUIRE(queue.size() == 0);
    REQUIRE(queue.empty());
}

TEST_CASE("BlockingQueue emplace constructs values in place", "[sync][blocking_queue]")
{
    BlockingQueue<std::pair<int, std::string>> queue;

    queue.emplace(7, "seven");
    queue.emplace(8, "eight");

    const auto first = queue.pop();
    const auto second = queue.pop();

    REQUIRE(first.first == 7);
    REQUIRE(first.second == "seven");
    REQUIRE(second.first == 8);
    REQUIRE(second.second == "eight");
}

TEST_CASE("BlockingQueue tryPush succeeds on unbounded queue", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue;

    REQUIRE(queue.tryPush(1));
    REQUIRE(queue.tryPush(2));
    REQUIRE(queue.size() == 2);

    REQUIRE(queue.pop() == 1);
    REQUIRE(queue.pop() == 2);
}

TEST_CASE("BlockingQueue tryPush succeeds when bounded queue has space", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue(2);

    REQUIRE(queue.tryPush(10));
    REQUIRE(queue.tryPush(20));
    REQUIRE(queue.size() == 2);
}

TEST_CASE("BlockingQueue tryPush fails when bounded queue is full", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue(2);

    REQUIRE(queue.tryPush(10));
    REQUIRE(queue.tryPush(20));
    REQUIRE_FALSE(queue.tryPush(30));

    REQUIRE(queue.size() == 2);
    REQUIRE(queue.pop() == 10);
    REQUIRE(queue.pop() == 20);
}

TEST_CASE("BlockingQueue tryPop returns nullopt when queue is empty", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue;

    const auto value = queue.tryPop();

    REQUIRE_FALSE(value.has_value());
}

TEST_CASE("BlockingQueue tryPop consumes front element and preserves FIFO", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue;

    queue.push(1);
    queue.push(2);

    const auto first = queue.tryPop();
    REQUIRE(first.has_value());
    REQUIRE(*first == 1);

    const auto second = queue.tryPop();
    REQUIRE(second.has_value());
    REQUIRE(*second == 2);

    REQUIRE(queue.empty());
}

TEST_CASE("BlockingQueue popFor returns value when producer arrives in time", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue;

    std::thread producer([&queue]()
                         {
                             std::this_thread::sleep_for(20ms);
                             queue.push(42); });

    const auto value = queue.popFor(200);

    REQUIRE(value.has_value());
    REQUIRE(*value == 42);

    producer.join();
}

TEST_CASE("BlockingQueue popFor times out when queue stays empty", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue;

    const auto value = queue.popFor(30);

    REQUIRE_FALSE(value.has_value());
    REQUIRE(queue.empty());
}

TEST_CASE("BlockingQueue popFor consumes front element and preserves FIFO", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue;

    queue.push(1);
    queue.push(2);
    queue.push(3);

    const auto first = queue.popFor(1);
    REQUIRE(first.has_value());
    REQUIRE(*first == 1);

    REQUIRE(queue.pop() == 2);
    REQUIRE(queue.pop() == 3);
    REQUIRE(queue.empty());
}

TEST_CASE("BlockingQueue push blocks on full bounded queue until consumer pops", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue(1);

    queue.push(1);

    std::atomic<bool> producerEntered{false};
    std::atomic<bool> producerFinished{false};

    std::thread producer([&queue, &producerEntered, &producerFinished]()
                         {
                             producerEntered.store(true, std::memory_order_release);
                             queue.push(2);
                             producerFinished.store(true, std::memory_order_release); });

    std::this_thread::sleep_for(30ms);

    REQUIRE(producerEntered.load(std::memory_order_acquire));
    REQUIRE_FALSE(producerFinished.load(std::memory_order_acquire));

    REQUIRE(queue.pop() == 1);

    producer.join();

    REQUIRE(producerFinished.load(std::memory_order_acquire));
    REQUIRE(queue.pop() == 2);
    REQUIRE(queue.empty());
}

TEST_CASE("BlockingQueue stop marks queue stopped", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue;

    REQUIRE_FALSE(queue.stopped());

    queue.stop();

    REQUIRE(queue.stopped());
}

TEST_CASE("BlockingQueue stop is idempotent", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue;

    queue.stop();
    REQUIRE(queue.stopped());

    REQUIRE_NOTHROW(queue.stop());
    REQUIRE(queue.stopped());
}

TEST_CASE("BlockingQueue stop causes push to throw", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue;

    queue.stop();

    REQUIRE_THROWS_AS(queue.push(1), std::runtime_error);
}

TEST_CASE("BlockingQueue stop causes emplace to throw", "[sync][blocking_queue]")
{
    BlockingQueue<std::string> queue;

    queue.stop();

    REQUIRE_THROWS_AS(queue.emplace("abc"), std::runtime_error);
}

TEST_CASE("BlockingQueue stop causes tryPush to fail", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue;

    queue.stop();

    REQUIRE_FALSE(queue.tryPush(1));
    REQUIRE(queue.empty());
}

TEST_CASE("BlockingQueue stop causes blocking pop on empty queue to throw", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue;

    queue.stop();

    REQUIRE_THROWS_AS(queue.pop(), std::runtime_error);
}

TEST_CASE("BlockingQueue stop causes popFor on empty queue to throw", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue;

    queue.stop();

    REQUIRE_THROWS_AS(static_cast<void>(queue.popFor(10)), std::runtime_error);
}

TEST_CASE("BlockingQueue stop still allows draining queued items", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue;

    queue.push(10);
    queue.push(20);
    queue.stop();

    REQUIRE(queue.stopped());

    REQUIRE(queue.pop() == 10);
    REQUIRE(queue.pop() == 20);
    REQUIRE_THROWS_AS(queue.pop(), std::runtime_error);
    REQUIRE_THROWS_AS(static_cast<void>(queue.popFor(1)), std::runtime_error);
}

TEST_CASE("BlockingQueue stop wakes waiting pop thread", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue;
    std::atomic<bool> exited{false};

    std::thread consumer([&queue, &exited]()
                         {
                             REQUIRE_THROWS_AS(queue.pop(), std::runtime_error);
                             exited.store(true, std::memory_order_release); });

    std::this_thread::sleep_for(30ms);
    REQUIRE_FALSE(exited.load(std::memory_order_acquire));

    queue.stop();
    consumer.join();

    REQUIRE(exited.load(std::memory_order_acquire));
}

TEST_CASE("BlockingQueue stop wakes waiting popFor thread", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue;
    std::atomic<bool> exited{false};

    std::thread consumer([&queue, &exited]()
                         {
                             REQUIRE_THROWS_AS(static_cast<void>(queue.popFor(1000)), std::runtime_error);
                             exited.store(true, std::memory_order_release); });

    std::this_thread::sleep_for(30ms);
    REQUIRE_FALSE(exited.load(std::memory_order_acquire));

    queue.stop();
    consumer.join();

    REQUIRE(exited.load(std::memory_order_acquire));
}

TEST_CASE("BlockingQueue stop wakes waiting push thread on full bounded queue", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue(1);
    queue.push(1);

    std::atomic<bool> producerStarted{false};
    std::atomic<bool> producerExited{false};

    std::thread producer([&queue, &producerStarted, &producerExited]()
                         {
                             producerStarted.store(true, std::memory_order_release);
                             REQUIRE_THROWS_AS(queue.push(2), std::runtime_error);
                             producerExited.store(true, std::memory_order_release); });

    std::this_thread::sleep_for(30ms);

    REQUIRE(producerStarted.load(std::memory_order_acquire));
    REQUIRE_FALSE(producerExited.load(std::memory_order_acquire));

    queue.stop();
    producer.join();

    REQUIRE(producerExited.load(std::memory_order_acquire));
    REQUIRE(queue.size() == 1);
    REQUIRE(queue.pop() == 1);
}

TEST_CASE("BlockingQueue single producer single consumer preserves all values", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue;
    std::vector<int> consumed;
    consumed.reserve(100);

    std::thread producer([&queue]()
                         {
                             for (int i = 0; i < 100; ++i)
                             {
                                 queue.push(i);
                             }
                             queue.stop(); });

    while (true)
    {
        try
        {
            const auto value = queue.popFor(20);
            if (value.has_value())
            {
                consumed.push_back(*value);
            }
        }
        catch (const std::runtime_error&)
        {
            break;
        }
    }

    producer.join();

    REQUIRE(consumed.size() == 100);
    for (int i = 0; i < 100; ++i)
    {
        REQUIRE(consumed[static_cast<std::size_t>(i)] == i);
    }
}

TEST_CASE("BlockingQueue multi producer single consumer keeps count integrity", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue;

    constexpr int producerCount = 4;
    constexpr int valuesPerProducer = 500;
    constexpr int totalValues = producerCount * valuesPerProducer;

    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};

    std::vector<std::thread> producers;
    producers.reserve(producerCount);

    for (int p = 0; p < producerCount; ++p)
    {
        producers.emplace_back([&queue, &produced, p]()
                               {
                                   const int base = p * valuesPerProducer;
                                   for (int i = 0; i < valuesPerProducer; ++i)
                                   {
                                       queue.push(base + i);
                                       produced.fetch_add(1, std::memory_order_relaxed);
                                   } });
    }

    std::thread consumer([&queue, &consumed]()
                         {
                             while (consumed.load(std::memory_order_relaxed) < totalValues)
                             {
                                 const auto value = queue.popFor(50);
                                 if (value.has_value())
                                 {
                                     consumed.fetch_add(1, std::memory_order_relaxed);
                                 }
                             } });

    for (auto& producer : producers)
    {
        producer.join();
    }

    consumer.join();

    REQUIRE(produced.load(std::memory_order_relaxed) == totalValues);
    REQUIRE(consumed.load(std::memory_order_relaxed) == totalValues);
    REQUIRE(queue.empty());
}

TEST_CASE("BlockingQueue size tracks pushes and pops", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue;

    REQUIRE(queue.size() == 0);

    queue.push(1);
    REQUIRE(queue.size() == 1);

    queue.push(2);
    REQUIRE(queue.size() == 2);

    static_cast<void>(queue.pop());
    REQUIRE(queue.size() == 1);

    static_cast<void>(queue.pop());
    REQUIRE(queue.size() == 0);
}

TEST_CASE("BlockingQueue empty reflects current queue state", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue;

    REQUIRE(queue.empty());

    queue.push(1);
    REQUIRE_FALSE(queue.empty());

    static_cast<void>(queue.pop());
    REQUIRE(queue.empty());
}

TEST_CASE("BlockingQueue draining after stop preserves FIFO order", "[sync][blocking_queue]")
{
    BlockingQueue<int> queue;

    for (int i = 0; i < 10; ++i)
    {
        queue.push(i);
    }

    queue.stop();

    for (int i = 0; i < 10; ++i)
    {
        REQUIRE(queue.pop() == i);
    }

    REQUIRE(queue.empty());
    REQUIRE_THROWS_AS(queue.pop(), std::runtime_error);
}

}  // namespace