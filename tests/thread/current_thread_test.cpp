#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <string>
#include <string_view>
#include <thread>

#include "dbase/thread/current_thread.h"

namespace
{
using namespace std::chrono_literals;
namespace current_thread = dbase::thread::current_thread;
}  // namespace

TEST_CASE("current_thread tid is nonzero", "[thread][current_thread]")
{
    REQUIRE(current_thread::tid() != 0);
}

TEST_CASE("current_thread name defaults to unknown in new thread", "[thread][current_thread]")
{
    std::string observed;

    std::thread worker([&]()
                       { observed = current_thread::name(); });

    worker.join();

    REQUIRE(observed == "unknown");
}

TEST_CASE("current_thread setName updates current thread local name", "[thread][current_thread]")
{
    current_thread::setName("dbase-main-test");

#if defined(__linux__) || defined(__APPLE__)
    REQUIRE(current_thread::name() == std::string_view("dbase-main-test").substr(0, 15));
#else
    REQUIRE(current_thread::name() == "dbase-main-test");
#endif
}

TEST_CASE("current_thread setName empty resets to unknown", "[thread][current_thread]")
{
    current_thread::setName("");
    REQUIRE(current_thread::name() == "unknown");
}

TEST_CASE("current_thread setName trims overlong names", "[thread][current_thread]")
{
    const std::string longName = "abcdefghijklmnopqrstuvwxyz0123456789";
    current_thread::setName(longName);

#if defined(__linux__) || defined(__APPLE__)
    REQUIRE(current_thread::name() == longName.substr(0, 15));
#else
    REQUIRE(current_thread::name() == longName.substr(0, 64));
#endif
}

TEST_CASE("current_thread name is thread local", "[thread][current_thread]")
{
    current_thread::setName("main-thread-name");
    std::string workerName;

    std::thread worker([&]()
                       {
                           current_thread::setName("worker-thread-name");
                           workerName = current_thread::name(); });

    worker.join();

#if defined(__linux__) || defined(__APPLE__)
    REQUIRE(current_thread::name() == std::string("main-thread-name").substr(0, 15));
    REQUIRE(workerName == std::string("worker-thread-name").substr(0, 15));
#else
    REQUIRE(current_thread::name() == "main-thread-name");
    REQUIRE(workerName == "worker-thread-name");
#endif
}

TEST_CASE("current_thread different threads have different tids", "[thread][current_thread]")
{
    const auto mainTid = current_thread::tid();
    std::uint64_t workerTid = 0;

    std::thread worker([&]()
                       { workerTid = current_thread::tid(); });

    worker.join();

    REQUIRE(workerTid != 0);
    REQUIRE(workerTid != mainTid);
}

TEST_CASE("current_thread sleepForMs sleeps for roughly requested duration", "[thread][current_thread]")
{
    const auto start = std::chrono::steady_clock::now();
    current_thread::sleepForMs(20);
    const auto elapsed = std::chrono::steady_clock::now() - start;

    REQUIRE(elapsed >= 10ms);
}

TEST_CASE("current_thread sleepForUs sleeps for roughly requested duration", "[thread][current_thread]")
{
    const auto start = std::chrono::steady_clock::now();
    current_thread::sleepForUs(500);
    const auto elapsed = std::chrono::steady_clock::now() - start;

    REQUIRE(elapsed >= 100us);
}