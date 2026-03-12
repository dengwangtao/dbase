#include <catch2/catch_test_macros.hpp>

#include <string>
#include <utility>
#include <vector>

#include "dbase/cache/lru_cache.h"
#include "dbase/error/error.h"

namespace
{
using dbase::ErrorCode;
using dbase::cache::LRUCache;

TEST_CASE("LRUCache constructor rejects zero capacity", "[cache][lru_cache]")
{
    using LRUCache = dbase::cache::LRUCache<int, int>;
    REQUIRE_THROWS_AS(LRUCache(0), std::invalid_argument);
}

TEST_CASE("LRUCache default state", "[cache][lru_cache]")
{
    LRUCache<int, int> cache(3);

    REQUIRE(cache.capacity() == 3);
    REQUIRE(cache.size() == 0);
    REQUIRE(cache.empty());
    REQUIRE_FALSE(cache.contains(1));
    REQUIRE_FALSE(cache.lru().has_value());
    REQUIRE_FALSE(cache.mru().has_value());
    REQUIRE_FALSE(cache.popLru().has_value());
    REQUIRE_FALSE(cache.popMru().has_value());
}

TEST_CASE("LRUCache put and get basic values", "[cache][lru_cache]")
{
    LRUCache<int, std::string> cache(3);

    cache.put(1, "one");
    cache.put(2, "two");

    REQUIRE(cache.size() == 2);
    REQUIRE_FALSE(cache.empty());
    REQUIRE(cache.contains(1));
    REQUIRE(cache.contains(2));

    const auto v1 = cache.get(1);
    REQUIRE(v1.has_value());
    REQUIRE(*v1 == "one");

    const auto v2 = cache.get(2);
    REQUIRE(v2.has_value());
    REQUIRE(*v2 == "two");

    REQUIRE_FALSE(cache.get(3).has_value());
}

TEST_CASE("LRUCache peek returns value without changing recency", "[cache][lru_cache]")
{
    LRUCache<int, std::string> cache(3);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    const auto lruBefore = cache.lru();
    const auto mruBefore = cache.mru();

    REQUIRE(lruBefore.has_value());
    REQUIRE(mruBefore.has_value());
    REQUIRE(lruBefore->first == 1);
    REQUIRE(mruBefore->first == 3);

    const auto peeked = cache.peek(1);
    REQUIRE(peeked.has_value());
    REQUIRE(*peeked == "one");

    const auto lruAfter = cache.lru();
    const auto mruAfter = cache.mru();

    REQUIRE(lruAfter.has_value());
    REQUIRE(mruAfter.has_value());
    REQUIRE(lruAfter->first == 1);
    REQUIRE(mruAfter->first == 3);
}

TEST_CASE("LRUCache get updates recency", "[cache][lru_cache]")
{
    LRUCache<int, std::string> cache(3);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    REQUIRE(cache.lru()->first == 1);
    REQUIRE(cache.mru()->first == 3);

    const auto value = cache.get(1);
    REQUIRE(value.has_value());
    REQUIRE(*value == "one");

    REQUIRE(cache.mru()->first == 1);
    REQUIRE(cache.lru()->first == 2);
}

TEST_CASE("LRUCache put existing key updates value and recency", "[cache][lru_cache]")
{
    LRUCache<int, std::string> cache(3);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    cache.put(2, "TWO");

    REQUIRE(cache.size() == 3);
    REQUIRE(cache.contains(2));

    const auto value = cache.get(2);
    REQUIRE(value.has_value());
    REQUIRE(*value == "TWO");

    REQUIRE(cache.mru()->first == 2);
    REQUIRE(cache.lru()->first == 1);
}

TEST_CASE("LRUCache evicts least recently used entry when capacity exceeded", "[cache][lru_cache]")
{
    LRUCache<int, std::string> cache(2);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    REQUIRE(cache.size() == 2);
    REQUIRE_FALSE(cache.contains(1));
    REQUIRE(cache.contains(2));
    REQUIRE(cache.contains(3));

    REQUIRE(cache.lru()->first == 2);
    REQUIRE(cache.mru()->first == 3);
}

TEST_CASE("LRUCache eviction respects get-updated recency", "[cache][lru_cache]")
{
    LRUCache<int, std::string> cache(2);

    cache.put(1, "one");
    cache.put(2, "two");

    const auto touched = cache.get(1);
    REQUIRE(touched.has_value());
    REQUIRE(*touched == "one");

    cache.put(3, "three");

    REQUIRE(cache.contains(1));
    REQUIRE_FALSE(cache.contains(2));
    REQUIRE(cache.contains(3));

    REQUIRE(cache.lru()->first == 1);
    REQUIRE(cache.mru()->first == 3);
}

TEST_CASE("LRUCache eviction callback is invoked with evicted entry", "[cache][lru_cache]")
{
    LRUCache<int, std::string> cache(2);

    std::vector<std::pair<int, std::string>> evicted;

    cache.setEvictCallback([&evicted](const int& key, const std::string& value)
                           { evicted.emplace_back(key, value); });

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");
    cache.put(4, "four");

    REQUIRE(evicted.size() == 2);
    REQUIRE(evicted[0] == std::pair<int, std::string>{1, "one"});
    REQUIRE(evicted[1] == std::pair<int, std::string>{2, "two"});
}

TEST_CASE("LRUCache remove erases existing key", "[cache][lru_cache]")
{
    LRUCache<int, std::string> cache(3);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    REQUIRE(cache.remove(2));
    REQUIRE_FALSE(cache.contains(2));
    REQUIRE(cache.size() == 2);
    REQUIRE_FALSE(cache.remove(2));

    REQUIRE(cache.contains(1));
    REQUIRE(cache.contains(3));
}

TEST_CASE("LRUCache popLru removes least recently used entry", "[cache][lru_cache]")
{
    LRUCache<int, std::string> cache(3);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    const auto popped = cache.popLru();

    REQUIRE(popped.has_value());
    REQUIRE(*popped == std::pair<int, std::string>{1, "one"});
    REQUIRE(cache.size() == 2);
    REQUIRE_FALSE(cache.contains(1));
    REQUIRE(cache.lru()->first == 2);
    REQUIRE(cache.mru()->first == 3);
}

TEST_CASE("LRUCache popMru removes most recently used entry", "[cache][lru_cache]")
{
    LRUCache<int, std::string> cache(3);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    const auto popped = cache.popMru();

    REQUIRE(popped.has_value());
    REQUIRE(*popped == std::pair<int, std::string>{3, "three"});
    REQUIRE(cache.size() == 2);
    REQUIRE_FALSE(cache.contains(3));
    REQUIRE(cache.lru()->first == 1);
    REQUIRE(cache.mru()->first == 2);
}

TEST_CASE("LRUCache lru and mru reflect current order", "[cache][lru_cache]")
{
    LRUCache<int, std::string> cache(4);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    REQUIRE(cache.lru().has_value());
    REQUIRE(cache.mru().has_value());
    REQUIRE(cache.lru()->first == 1);
    REQUIRE(cache.mru()->first == 3);

    static_cast<void>(cache.get(1));

    REQUIRE(cache.lru()->first == 2);
    REQUIRE(cache.mru()->first == 1);

    cache.put(4, "four");

    REQUIRE(cache.lru()->first == 2);
    REQUIRE(cache.mru()->first == 4);
}

TEST_CASE("LRUCache clear removes all entries", "[cache][lru_cache]")
{
    LRUCache<int, std::string> cache(3);

    cache.put(1, "one");
    cache.put(2, "two");

    REQUIRE(cache.size() == 2);

    cache.clear();

    REQUIRE(cache.size() == 0);
    REQUIRE(cache.empty());
    REQUIRE_FALSE(cache.contains(1));
    REQUIRE_FALSE(cache.contains(2));
    REQUIRE_FALSE(cache.lru().has_value());
    REQUIRE_FALSE(cache.mru().has_value());
    REQUIRE_FALSE(cache.popLru().has_value());
    REQUIRE_FALSE(cache.popMru().has_value());
}

TEST_CASE("LRUCache getOrError returns value for existing key", "[cache][lru_cache]")
{
    LRUCache<int, std::string> cache(2);

    cache.put(1, "one");

    auto result = cache.getOrError(1);

    REQUIRE(result.hasValue());
    REQUIRE(result.value() == "one");
}

TEST_CASE("LRUCache getOrError returns NotFound for missing key", "[cache][lru_cache]")
{
    LRUCache<int, std::string> cache(2);

    auto result = cache.getOrError(42);

    REQUIRE(result.hasError());
    REQUIRE(result.error().code() == ErrorCode::NotFound);
}

TEST_CASE("LRUCache supports move put overloads", "[cache][lru_cache]")
{
    LRUCache<std::string, std::string> cache(2);

    std::string key = "k1";
    std::string value = "v1";

    cache.put(std::move(key), std::move(value));

    REQUIRE(cache.contains("k1"));

    const auto result = cache.get("k1");
    REQUIRE(result.has_value());
    REQUIRE(*result == "v1");
}

TEST_CASE("LRUCache remove updates order of remaining items correctly", "[cache][lru_cache]")
{
    LRUCache<int, std::string> cache(4);

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");
    cache.put(4, "four");

    REQUIRE(cache.remove(2));

    REQUIRE(cache.size() == 3);
    REQUIRE(cache.lru()->first == 1);
    REQUIRE(cache.mru()->first == 4);

    static_cast<void>(cache.get(1));
    REQUIRE(cache.lru()->first == 3);
    REQUIRE(cache.mru()->first == 1);
}

TEST_CASE("LRUCache pop operations on empty cache return nullopt", "[cache][lru_cache]")
{
    LRUCache<int, int> cache(1);

    REQUIRE_FALSE(cache.popLru().has_value());
    REQUIRE_FALSE(cache.popMru().has_value());
}

TEST_CASE("LRUCache single element behaves as both LRU and MRU", "[cache][lru_cache]")
{
    LRUCache<int, std::string> cache(1);

    cache.put(7, "seven");

    REQUIRE(cache.lru().has_value());
    REQUIRE(cache.mru().has_value());
    REQUIRE(cache.lru()->first == 7);
    REQUIRE(cache.mru()->first == 7);

    const auto popped = cache.popLru();
    REQUIRE(popped.has_value());
    REQUIRE(*popped == std::pair<int, std::string>{7, "seven"});

    REQUIRE(cache.empty());
}
}  // namespace