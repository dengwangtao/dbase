#include <catch2/catch_test_macros.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

#include "dbase/random/random.h"

namespace
{
namespace rnd = dbase::random;
}

TEST_CASE("Random uniformInt returns value within range", "[random]")
{
    rnd::Random random;

    for (int i = 0; i < 100; ++i)
    {
        const auto value = random.uniformInt(-3, 7);
        REQUIRE(value >= -3);
        REQUIRE(value <= 7);
    }
}

TEST_CASE("Random uniformInt64 returns value within range", "[random]")
{
    rnd::Random random;

    for (int i = 0; i < 100; ++i)
    {
        const auto value = random.uniformInt64(-100, 100);
        REQUIRE(value >= -100);
        REQUIRE(value <= 100);
    }
}

TEST_CASE("Random uniformReal returns value within range", "[random]")
{
    rnd::Random random;

    for (int i = 0; i < 100; ++i)
    {
        const auto value = random.uniformReal(-1.5, 2.5);
        REQUIRE(value >= -1.5);
        REQUIRE(value <= 2.5);
    }
}

TEST_CASE("Random bernoulli returns boolean values", "[random]")
{
    rnd::Random random;

    for (int i = 0; i < 100; ++i)
    {
        const bool value = random.bernoulli(0.5);
        REQUIRE((value == true || value == false));
    }
}

TEST_CASE("Random invalid ranges throw", "[random]")
{
    rnd::Random random;

    REQUIRE_THROWS_AS(random.uniformInt(5, 4), std::invalid_argument);
    REQUIRE_THROWS_AS(random.uniformInt64(5, 4), std::invalid_argument);
    REQUIRE_THROWS_AS(random.uniformReal(2.0, 1.0), std::invalid_argument);
}

TEST_CASE("Random bernoulli invalid probability throws", "[random]")
{
    rnd::Random random;

    REQUIRE_THROWS_AS(random.bernoulli(-0.1), std::invalid_argument);
    REQUIRE_THROWS_AS(random.bernoulli(1.1), std::invalid_argument);
}

TEST_CASE("Random choice returns one of the provided items", "[random]")
{
    rnd::Random random;
    const std::array<int, 4> items{10, 20, 30, 40};

    for (int i = 0; i < 100; ++i)
    {
        const int value = random.choice(std::span<const int>(items));
        REQUIRE((value == 10 || value == 20 || value == 30 || value == 40));
    }
}

TEST_CASE("Random choice throws on empty input", "[random]")
{
    rnd::Random random;
    const std::vector<int> items;

    REQUIRE_THROWS_AS(random.choice(std::span<const int>(items)), std::invalid_argument);
}

TEST_CASE("Random weightedIndex returns index within bounds", "[random]")
{
    rnd::Random random;
    const std::array<double, 3> weights{1.0, 2.0, 3.0};

    for (int i = 0; i < 100; ++i)
    {
        const std::size_t index = random.weightedIndex(std::span<const double>(weights));
        REQUIRE(index < weights.size());
    }
}

TEST_CASE("Random weightedIndex throws on invalid weights", "[random]")
{
    rnd::Random random;
    const std::vector<double> empty;
    const std::array<double, 3> negative{1.0, -1.0, 2.0};
    const std::array<double, 3> zeroes{0.0, 0.0, 0.0};

    REQUIRE_THROWS_AS(random.weightedIndex(std::span<const double>(empty)), std::invalid_argument);
    REQUIRE_THROWS_AS(random.weightedIndex(std::span<const double>(negative)), std::invalid_argument);
    REQUIRE_THROWS_AS(random.weightedIndex(std::span<const double>(zeroes)), std::invalid_argument);
}

TEST_CASE("threadLocal helpers are callable", "[random]")
{
    auto& rng = rnd::threadLocal();

    const auto u64 = rng.nextU64();
    const auto u32 = rng.nextU32();
    const auto i32 = rnd::uniformInt(1, 3);
    const auto i64 = rnd::uniformInt64(1, 3);
    const auto real = rnd::uniformReal(0.0, 1.0);
    const auto bit = rnd::bernoulli(0.5);

    REQUIRE(u64 >= 0);
    REQUIRE(u32 >= 0);
    REQUIRE(i32 >= 1);
    REQUIRE(i32 <= 3);
    REQUIRE(i64 >= 1);
    REQUIRE(i64 <= 3);
    REQUIRE(real >= 0.0);
    REQUIRE(real <= 1.0);
    REQUIRE((bit == true || bit == false));
}