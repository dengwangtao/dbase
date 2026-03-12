#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <string>
#include <utility>

#include "dbase/memory/object_pool.h"

namespace
{
using dbase::memory::ObjectPool;

struct Dummy
{
        int value{0};
        std::string text;
};

struct Trackable
{
        static inline int ctorCount = 0;
        static inline int dtorCount = 0;

        int value{0};

        Trackable()
        {
            ++ctorCount;
        }

        explicit Trackable(int v)
            : value(v)
        {
            ++ctorCount;
        }

        ~Trackable()
        {
            ++dtorCount;
        }

        static void resetCounters()
        {
            ctorCount = 0;
            dtorCount = 0;
        }
};

TEST_CASE("ObjectPool default state", "[memory][object_pool]")
{
    ObjectPool<Dummy> pool;

    REQUIRE(pool.maxIdle() == 1024);
    REQUIRE(pool.idleCount() == 0);
    REQUIRE(pool.empty());

    const auto stats = pool.stats();
    REQUIRE(stats.totalCreated == 0);
    REQUIRE(stats.totalAcquired == 0);
    REQUIRE(stats.totalReleased == 0);
    REQUIRE(stats.idleCount == 0);
    REQUIRE(stats.maxIdle == 1024);
}

TEST_CASE("ObjectPool acquire creates object when idle pool is empty", "[memory][object_pool]")
{
    ObjectPool<Dummy> pool(4);

    auto obj = pool.acquire();

    REQUIRE(obj != nullptr);
    REQUIRE(pool.idleCount() == 0);
    REQUIRE(pool.empty());

    const auto stats = pool.stats();
    REQUIRE(stats.totalCreated == 1);
    REQUIRE(stats.totalAcquired == 1);
    REQUIRE(stats.totalReleased == 0);
    REQUIRE(stats.idleCount == 0);
    REQUIRE(stats.maxIdle == 4);
}

TEST_CASE("ObjectPool release returns object to idle pool", "[memory][object_pool]")
{
    ObjectPool<Dummy> pool(4);

    auto obj = pool.acquire();
    obj->value = 42;
    pool.release(std::move(obj));

    REQUIRE(obj == nullptr);
    REQUIRE(pool.idleCount() == 1);
    REQUIRE_FALSE(pool.empty());

    const auto stats = pool.stats();
    REQUIRE(stats.totalCreated == 1);
    REQUIRE(stats.totalAcquired == 1);
    REQUIRE(stats.totalReleased == 1);
    REQUIRE(stats.idleCount == 1);
    REQUIRE(stats.maxIdle == 4);
}

TEST_CASE("ObjectPool acquire reuses idle object", "[memory][object_pool]")
{
    ObjectPool<Dummy> pool(4);

    auto first = pool.acquire();
    auto* raw = first.get();
    first->value = 123;
    pool.release(std::move(first));

    REQUIRE(pool.idleCount() == 1);

    auto second = pool.acquire();

    REQUIRE(second != nullptr);
    REQUIRE(second.get() == raw);
    REQUIRE(second->value == 123);
    REQUIRE(pool.idleCount() == 0);

    const auto stats = pool.stats();
    REQUIRE(stats.totalCreated == 1);
    REQUIRE(stats.totalAcquired == 2);
    REQUIRE(stats.totalReleased == 1);
    REQUIRE(stats.idleCount == 0);
}

TEST_CASE("ObjectPool release nullptr is no-op", "[memory][object_pool]")
{
    ObjectPool<Dummy> pool(4);
    std::unique_ptr<Dummy> obj;

    pool.release(std::move(obj));

    REQUIRE(pool.idleCount() == 0);
    REQUIRE(pool.empty());

    const auto stats = pool.stats();
    REQUIRE(stats.totalCreated == 0);
    REQUIRE(stats.totalAcquired == 0);
    REQUIRE(stats.totalReleased == 0);
    REQUIRE(stats.idleCount == 0);
}

TEST_CASE("ObjectPool release respects maxIdle limit", "[memory][object_pool]")
{
    Trackable::resetCounters();

    {
        ObjectPool<Trackable> pool(2);

        auto a = pool.acquire();
        auto b = pool.acquire();
        auto c = pool.acquire();

        REQUIRE(pool.idleCount() == 0);

        pool.release(std::move(a));
        pool.release(std::move(b));
        pool.release(std::move(c));

        REQUIRE(pool.idleCount() == 2);

        const auto stats = pool.stats();
        REQUIRE(stats.totalCreated == 3);
        REQUIRE(stats.totalAcquired == 3);
        REQUIRE(stats.totalReleased == 3);
        REQUIRE(stats.idleCount == 2);
        REQUIRE(stats.maxIdle == 2);
    }

    REQUIRE(Trackable::ctorCount == 3);
    REQUIRE(Trackable::dtorCount == 3);
}

TEST_CASE("ObjectPool reserve pre-creates idle objects up to requested count", "[memory][object_pool]")
{
    ObjectPool<Dummy> pool(4);

    pool.reserve(3);

    REQUIRE(pool.idleCount() == 3);
    REQUIRE_FALSE(pool.empty());

    const auto stats = pool.stats();
    REQUIRE(stats.totalCreated == 3);
    REQUIRE(stats.totalAcquired == 0);
    REQUIRE(stats.totalReleased == 0);
    REQUIRE(stats.idleCount == 3);
    REQUIRE(stats.maxIdle == 4);
}

TEST_CASE("ObjectPool reserve does not exceed maxIdle", "[memory][object_pool]")
{
    ObjectPool<Dummy> pool(2);

    pool.reserve(10);

    REQUIRE(pool.idleCount() == 2);

    const auto stats = pool.stats();
    REQUIRE(stats.totalCreated == 2);
    REQUIRE(stats.totalAcquired == 0);
    REQUIRE(stats.totalReleased == 0);
    REQUIRE(stats.idleCount == 2);
    REQUIRE(stats.maxIdle == 2);
}

TEST_CASE("ObjectPool reserve on partially filled pool only tops up", "[memory][object_pool]")
{
    ObjectPool<Dummy> pool(4);

    pool.reserve(2);
    REQUIRE(pool.idleCount() == 2);

    auto obj = pool.acquire();
    REQUIRE(pool.idleCount() == 1);

    pool.reserve(3);
    REQUIRE(pool.idleCount() == 3);

    const auto stats = pool.stats();
    REQUIRE(stats.totalCreated == 4);
    REQUIRE(stats.totalAcquired == 1);
    REQUIRE(stats.totalReleased == 0);
    REQUIRE(stats.idleCount == 3);
}

TEST_CASE("ObjectPool clear removes all idle objects", "[memory][object_pool]")
{
    Trackable::resetCounters();

    {
        ObjectPool<Trackable> pool(4);
        pool.reserve(3);

        REQUIRE(pool.idleCount() == 3);

        pool.clear();

        REQUIRE(pool.idleCount() == 0);
        REQUIRE(pool.empty());

        const auto stats = pool.stats();
        REQUIRE(stats.totalCreated == 3);
        REQUIRE(stats.totalAcquired == 0);
        REQUIRE(stats.totalReleased == 0);
        REQUIRE(stats.idleCount == 0);
    }

    REQUIRE(Trackable::ctorCount == 3);
    REQUIRE(Trackable::dtorCount == 3);
}

TEST_CASE("ObjectPool create constructs custom object and updates counters", "[memory][object_pool]")
{
    ObjectPool<Dummy> pool(4);

    auto obj = pool.create();
    REQUIRE(obj != nullptr);
    REQUIRE(obj->value == 0);
    REQUIRE(obj->text.empty());

    auto obj2 = pool.create();
    obj2->value = 7;
    obj2->text = "hello";

    REQUIRE(obj2->value == 7);
    REQUIRE(obj2->text == "hello");

    const auto stats = pool.stats();
    REQUIRE(stats.totalCreated == 2);
    REQUIRE(stats.totalAcquired == 0);
    REQUIRE(stats.totalReleased == 0);
    REQUIRE(stats.idleCount == 0);
}

TEST_CASE("ObjectPool create forwards constructor arguments", "[memory][object_pool]")
{
    ObjectPool<Trackable> pool(4);

    auto obj = pool.create(99);

    REQUIRE(obj != nullptr);
    REQUIRE(obj->value == 99);

    const auto stats = pool.stats();
    REQUIRE(stats.totalCreated == 1);
    REQUIRE(stats.totalAcquired == 0);
    REQUIRE(stats.totalReleased == 0);
    REQUIRE(stats.idleCount == 0);
}

TEST_CASE("ObjectPool reset callback is invoked on release", "[memory][object_pool]")
{
    ObjectPool<Dummy> pool(4);

    int resetCount = 0;
    pool.setResetCallback([&resetCount](Dummy& obj)
                          {
                              ++resetCount;
                              obj.value = 0;
                              obj.text.clear(); });

    auto obj = pool.acquire();
    obj->value = 42;
    obj->text = "dirty";

    pool.release(std::move(obj));

    REQUIRE(resetCount == 1);
    REQUIRE(pool.idleCount() == 1);

    auto reused = pool.acquire();
    REQUIRE(reused->value == 0);
    REQUIRE(reused->text.empty());
}

TEST_CASE("ObjectPool factory callback is used to create objects", "[memory][object_pool]")
{
    ObjectPool<Dummy> pool(4);

    int factoryCount = 0;
    pool.setFactoryCallback([&factoryCount]()
                            {
                                ++factoryCount;
                                auto ptr = std::make_unique<Dummy>();
                                ptr->value = 77;
                                ptr->text = "factory";
                                return ptr; });

    auto obj = pool.acquire();

    REQUIRE(factoryCount == 1);
    REQUIRE(obj != nullptr);
    REQUIRE(obj->value == 77);
    REQUIRE(obj->text == "factory");

    const auto stats = pool.stats();
    REQUIRE(stats.totalCreated == 1);
    REQUIRE(stats.totalAcquired == 1);
    REQUIRE(stats.totalReleased == 0);
}

TEST_CASE("ObjectPool falls back to default construction when factory returns nullptr", "[memory][object_pool]")
{
    ObjectPool<Dummy> pool(4);

    int factoryCount = 0;
    pool.setFactoryCallback([&factoryCount]()
                            {
                                ++factoryCount;
                                return std::unique_ptr<Dummy>{}; });

    auto obj = pool.acquire();

    REQUIRE(factoryCount == 1);
    REQUIRE(obj != nullptr);
    REQUIRE(obj->value == 0);
    REQUIRE(obj->text.empty());

    const auto stats = pool.stats();
    REQUIRE(stats.totalCreated == 1);
    REQUIRE(stats.totalAcquired == 1);
    REQUIRE(stats.totalReleased == 0);
}

TEST_CASE("ObjectPool makeShared acquires object and returns it to pool on destruction", "[memory][object_pool]")
{
    ObjectPool<Dummy> pool(4);

    Dummy* raw = nullptr;

    {
        auto shared = pool.makeShared();
        REQUIRE(shared != nullptr);
        raw = shared.get();
        shared->value = 55;

        REQUIRE(pool.idleCount() == 0);

        const auto stats = pool.stats();
        REQUIRE(stats.totalCreated == 1);
        REQUIRE(stats.totalAcquired == 1);
        REQUIRE(stats.totalReleased == 0);
    }

    REQUIRE(pool.idleCount() == 1);

    auto reused = pool.acquire();
    REQUIRE(reused.get() == raw);
    REQUIRE(reused->value == 55);

    const auto stats = pool.stats();
    REQUIRE(stats.totalCreated == 1);
    REQUIRE(stats.totalAcquired == 2);
    REQUIRE(stats.totalReleased == 1);
}

TEST_CASE("ObjectPool stats reflect mixed reserve acquire release flow", "[memory][object_pool]")
{
    ObjectPool<Dummy> pool(3);

    pool.reserve(2);

    auto a = pool.acquire();
    auto b = pool.acquire();
    auto c = pool.acquire();

    pool.release(std::move(a));
    pool.release(std::move(b));
    pool.release(std::move(c));

    const auto stats = pool.stats();
    REQUIRE(stats.totalCreated == 3);
    REQUIRE(stats.totalAcquired == 3);
    REQUIRE(stats.totalReleased == 3);
    REQUIRE(stats.idleCount == 3);
    REQUIRE(stats.maxIdle == 3);
}

TEST_CASE("ObjectPool empty reflects idle pool state", "[memory][object_pool]")
{
    ObjectPool<Dummy> pool(2);

    REQUIRE(pool.empty());

    pool.reserve(1);
    REQUIRE_FALSE(pool.empty());

    auto obj = pool.acquire();
    REQUIRE(pool.empty());

    pool.release(std::move(obj));
    REQUIRE_FALSE(pool.empty());
}


TEST_CASE("ObjectPool with zero maxIdle never stores released objects", "[memory][object_pool]")
{
    Trackable::resetCounters();

    {
        ObjectPool<Trackable> pool(0);

        auto obj = pool.acquire();
        REQUIRE(obj != nullptr);

        pool.release(std::move(obj));

        REQUIRE(pool.idleCount() == 0);
        REQUIRE(pool.empty());

        const auto stats = pool.stats();
        REQUIRE(stats.totalCreated == 1);
        REQUIRE(stats.totalAcquired == 1);
        REQUIRE(stats.totalReleased == 1);
        REQUIRE(stats.idleCount == 0);
        REQUIRE(stats.maxIdle == 0);
    }

    REQUIRE(Trackable::ctorCount == 1);
    REQUIRE(Trackable::dtorCount == 1);
}

TEST_CASE("ObjectPool reserve zero is no-op", "[memory][object_pool]")
{
    ObjectPool<Dummy> pool(4);

    pool.reserve(0);

    REQUIRE(pool.idleCount() == 0);
    REQUIRE(pool.empty());

    const auto stats = pool.stats();
    REQUIRE(stats.totalCreated == 0);
    REQUIRE(stats.totalAcquired == 0);
    REQUIRE(stats.totalReleased == 0);
    REQUIRE(stats.idleCount == 0);
}

TEST_CASE("ObjectPool makeShared remains safe after pool destruction", "[memory][object_pool]")
{
    Trackable::resetCounters();

    std::shared_ptr<Trackable> shared;

    {
        ObjectPool<Trackable> pool(4);
        shared = pool.makeShared();

        REQUIRE(shared != nullptr);

        const auto stats = pool.stats();
        REQUIRE(stats.totalCreated == 1);
        REQUIRE(stats.totalAcquired == 1);
        REQUIRE(stats.totalReleased == 0);
    }

    REQUIRE(Trackable::ctorCount == 1);
    REQUIRE(Trackable::dtorCount == 0);

    shared.reset();

    REQUIRE(Trackable::dtorCount == 1);
}

}  // namespace