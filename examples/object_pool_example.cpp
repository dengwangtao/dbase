#include "dbase/log/log.h"
#include "dbase/memory/object_pool.h"

#include <memory>
#include <string>
#include <vector>

namespace
{
struct Message
{
        std::string text;
        int id{0};
};
}  // namespace

int main()
{
    dbase::log::setDefaultLevel(dbase::log::Level::Trace);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::Source);

    dbase::memory::ObjectPool<Message> pool(8);

    pool.setResetCallback([](Message& msg)
                          {
        msg.text.clear();
        msg.id = 0; });

    pool.reserve(4);

    {
        auto obj1 = pool.acquire();
        obj1->id = 1;
        obj1->text = "hello";

        DBASE_LOG_INFO("obj1 id={} text={}", obj1->id, obj1->text);

        pool.release(std::move(obj1));
    }

    {
        auto obj2 = pool.acquire();
        DBASE_LOG_INFO("obj2 reused id={} text='{}'", obj2->id, obj2->text);
        obj2->id = 2;
        obj2->text = "world";
        pool.release(std::move(obj2));
    }

    {
        auto shared = pool.makeShared();
        shared->id = 3;
        shared->text = "shared-object";
        DBASE_LOG_INFO("shared id={} text={}", shared->id, shared->text);
    }

    {
        std::vector<std::unique_ptr<Message>> items;
        for (int i = 0; i < 6; ++i)
        {
            auto obj = pool.acquire();
            obj->id = i + 10;
            obj->text = "bulk-" + std::to_string(i);
            items.emplace_back(std::move(obj));
        }

        for (auto& item : items)
        {
            pool.release(std::move(item));
        }
    }

    const auto stats = pool.stats();
    DBASE_LOG_INFO("totalCreated={}", stats.totalCreated);
    DBASE_LOG_INFO("totalAcquired={}", stats.totalAcquired);
    DBASE_LOG_INFO("totalReleased={}", stats.totalReleased);
    DBASE_LOG_INFO("idleCount={}", stats.idleCount);
    DBASE_LOG_INFO("maxIdle={}", stats.maxIdle);

    pool.clear();
    DBASE_LOG_INFO("after clear idleCount={}", pool.idleCount());

    return 0;
}