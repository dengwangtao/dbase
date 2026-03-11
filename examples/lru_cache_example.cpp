#include "dbase/cache/lru_cache.h"
#include "dbase/log/log.h"

#include <string>

int main()
{
    dbase::log::setDefaultLevel(dbase::log::Level::Trace);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::Source);

    dbase::cache::LRUCache<int, std::string> cache(3);

    cache.setEvictCallback([](const int& key, const std::string& value)
                           { DBASE_LOG_INFO("evict key={} value={}", key, value); });

    cache.put(1, "one");
    cache.put(2, "two");
    cache.put(3, "three");

    DBASE_LOG_INFO("size={}", cache.size());

    auto v1 = cache.get(1);
    if (v1)
    {
        DBASE_LOG_INFO("get 1 => {}", *v1);
    }

    cache.put(4, "four");
    cache.put(5, "five");

    DBASE_LOG_INFO("contains 1 => {}", cache.contains(1));
    DBASE_LOG_INFO("contains 2 => {}", cache.contains(2));
    DBASE_LOG_INFO("contains 3 => {}", cache.contains(3));
    DBASE_LOG_INFO("contains 4 => {}", cache.contains(4));
    DBASE_LOG_INFO("contains 5 => {}", cache.contains(5));

    auto peek3 = cache.peek(3);
    if (peek3)
    {
        DBASE_LOG_INFO("peek 3 => {}", *peek3);
    }

    auto mru = cache.mru();
    if (mru)
    {
        DBASE_LOG_INFO("mru key={} value={}", mru->first, mru->second);
    }

    auto lru = cache.lru();
    if (lru)
    {
        DBASE_LOG_INFO("lru key={} value={}", lru->first, lru->second);
    }

    auto r4 = cache.getOrError(4);
    if (r4)
    {
        DBASE_LOG_INFO("getOrError 4 => {}", r4.value());
    }

    auto r2 = cache.getOrError(2);
    if (!r2)
    {
        DBASE_LOG_ERROR("getOrError 2 failed: {}", r2.error().toString());
    }

    (void)cache.remove(1);
    DBASE_LOG_INFO("after remove 1, contains 1 => {}", cache.contains(1));

    auto popped = cache.popLru();
    if (popped)
    {
        DBASE_LOG_INFO("popLru key={} value={}", popped->first, popped->second);
    }

    DBASE_LOG_INFO("final size={}", cache.size());

    cache.clear();
    DBASE_LOG_INFO("after clear size={}", cache.size());

    return 0;
}