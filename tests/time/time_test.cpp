#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <catch2/catch_approx.hpp>

#include <chrono>
#include <cstdint>
#include <string>

#include "dbase/time/time.h"

namespace
{
using namespace std::chrono_literals;
namespace dtime = dbase::time;
}  // namespace

TEST_CASE("time now clocks are monotonic enough in immediate succession", "[time]")
{
    const auto ns1 = dtime::nowNs();
    const auto us1 = dtime::nowUs();
    const auto ms1 = dtime::nowMs();

    const auto ns2 = dtime::nowNs();
    const auto us2 = dtime::nowUs();
    const auto ms2 = dtime::nowMs();

    REQUIRE(ns2 >= ns1);
    REQUIRE(us2 >= us1);
    REQUIRE(ms2 >= ms1);
}

TEST_CASE("time steady clocks are monotonic enough in immediate succession", "[time]")
{
    const auto ns1 = dtime::steadyNowNs();
    const auto us1 = dtime::steadyNowUs();
    const auto ms1 = dtime::steadyNowMs();

    const auto ns2 = dtime::steadyNowNs();
    const auto us2 = dtime::steadyNowUs();
    const auto ms2 = dtime::steadyNowMs();

    REQUIRE(ns2 >= ns1);
    REQUIRE(us2 >= us1);
    REQUIRE(ms2 >= ms1);
}

TEST_CASE("time formatNow returns non empty text", "[time]")
{
    const auto text = dtime::formatNow();

    REQUIRE_FALSE(text.empty());
}

TEST_CASE("time formatTimestampMs formats epoch correctly", "[time]")
{
    const auto text = dtime::formatTimestampMs(0, "%Y");

    REQUIRE(text == "1970");
}

TEST_CASE("time formatTimestampUs formats epoch correctly", "[time]")
{
    const auto text = dtime::formatTimestampUs(0, "%Y");

    REQUIRE(text == "1970");
}

TEST_CASE("time duration conversion helpers convert values correctly", "[time]")
{
    REQUIRE(dtime::toNs(1ms) == 1'000'000);
    REQUIRE(dtime::toUs(1ms) == 1'000);
    REQUIRE(dtime::toMs(1500us) == 1);
    REQUIRE(dtime::toSeconds(1500ms) == Catch::Approx(1.5));
}

TEST_CASE("time sleepForNs sleeps for at least a tiny duration", "[time]")
{
    const auto begin = std::chrono::steady_clock::now();
    dtime::sleepForNs(200'000);
    const auto elapsed = std::chrono::steady_clock::now() - begin;

    REQUIRE(elapsed >= 50us);
}

TEST_CASE("time sleepForUs sleeps for at least a tiny duration", "[time]")
{
    const auto begin = std::chrono::steady_clock::now();
    dtime::sleepForUs(500);
    const auto elapsed = std::chrono::steady_clock::now() - begin;

    REQUIRE(elapsed >= 100us);
}

TEST_CASE("time sleepForMs sleeps for at least requested-ish duration", "[time]")
{
    const auto begin = std::chrono::steady_clock::now();
    dtime::sleepForMs(20);
    const auto elapsed = std::chrono::steady_clock::now() - begin;

    REQUIRE(elapsed >= 10ms);
}

TEST_CASE("time sleepUntil waits until future time point", "[time]")
{
    const auto target = std::chrono::steady_clock::now() + 20ms;
    dtime::sleepUntil(target);
    const auto now = std::chrono::steady_clock::now();

    REQUIRE(now >= target);
}

TEST_CASE("Timestamp now returns nonzero current unix microseconds", "[time]")
{
    const auto ts = dtime::Timestamp::now();

    REQUIRE(ts.unixUs() > 0);
}

TEST_CASE("Timestamp format uses stored microsecond timestamp", "[time]")
{
    const dtime::Timestamp ts(0);

    REQUIRE(ts.format("%Y") == "1970");
}

TEST_CASE("Stopwatch starts near zero and grows with time", "[time]")
{
    dtime::Stopwatch sw;

    REQUIRE(sw.elapsedNs() >= 0);
    REQUIRE(sw.elapsedUs() >= 0);
    REQUIRE(sw.elapsedMs() >= 0);
    REQUIRE(sw.elapsedSeconds() >= 0.0);

    dtime::sleepForMs(20);

    REQUIRE(sw.elapsedNs() > 0);
    REQUIRE(sw.elapsedUs() > 0);
    REQUIRE(sw.elapsedMs() >= 10);
    REQUIRE(sw.elapsedSeconds() > 0.0);
}

TEST_CASE("Stopwatch reset restarts elapsed measurement", "[time]")
{
    dtime::Stopwatch sw;

    dtime::sleepForMs(20);
    const auto beforeReset = sw.elapsedMs();
    REQUIRE(beforeReset >= 10);

    sw.reset();
    const auto afterReset = sw.elapsedMs();

    REQUIRE(afterReset >= 0);
    REQUIRE(afterReset <= beforeReset);
}

TEST_CASE("time formatted current timestamp has expected separator pattern", "[time]")
{
    const auto text = dtime::formatNow("%Y-%m-%d %H:%M:%S");

    REQUIRE(text.size() == 19);
    REQUIRE(text[4] == '-');
    REQUIRE(text[7] == '-');
    REQUIRE(text[10] == ' ');
    REQUIRE(text[13] == ':');
    REQUIRE(text[16] == ':');
}