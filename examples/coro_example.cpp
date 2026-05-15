#include "dbase/coro/coroutine_scheduler.h"
#include "dbase/log/log.h"
#include <chrono>
#include <memory>
#include <string>
#include <thread>

using dbase::coro::CoroutineScheduler;
using dbase::coro::Task;

static Task<void> Worker(CoroutineScheduler& sched, std::shared_ptr<uint64_t> id_ref, std::shared_ptr<bool> done, std::string name, int32_t steps)
{
    DBASE_LOG_INFO("[{}] start, IsInCoroutine={}", name, sched.IsInCoroutine());

    for (int32_t i = 1; i <= steps; ++i)
    {
        DBASE_LOG_INFO("[{}] step {} co_id={} IsInCoroutine={} -> yield", name, i, *id_ref, sched.IsInCoroutine());

        // 把“真正挂起点 handle”记录到 scheduler.current
        co_await sched.Yield();
    }

    DBASE_LOG_INFO("[{}] finish, IsInCoroutine={}", name, sched.IsInCoroutine());
    *done = true;
    co_return;
}

int32_t main()
{
    CoroutineScheduler sched;
    sched.Init();

    DBASE_LOG_INFO("[main] start, IsInCoroutine={}", sched.IsInCoroutine());

    // === 创建两个协程 ===
    auto id1_ref = std::make_shared<uint64_t>(0);
    auto id2_ref = std::make_shared<uint64_t>(0);

    auto done1 = std::make_shared<bool>(false);
    auto done2 = std::make_shared<bool>(false);

    uint64_t id1 = 0, id2 = 0;

    sched.Create(Worker(sched, id1_ref, done1, "A", 10), id1);
    *id1_ref = id1;

    sched.Create(Worker(sched, id2_ref, done2, "B", 2), id2);
    *id2_ref = id2;

    // === 主循环：每帧 resume 一次
    int32_t tick = 0;
    while (!(*done1 && *done2))
    {
        ++tick;
        DBASE_LOG_INFO("[main] tick {} co_id1={} co_id2={} IsInCoroutine={}", tick, id1, id2, sched.IsInCoroutine());

        // 每次 Resume，只会跑到下一次 yield 处
        sched.Resume(id1);
        sched.Resume(id2);

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    DBASE_LOG_INFO("[main] both done. now demonstrate Restart on id1...");

    // Restart：复用同一个 id，换一棵新 coroutine 栈 ===
    *done1 = false;
    sched.Restart(id1, Worker(sched, id1_ref, done1, "A-restart", 3));

    tick = 0;
    while (!*done1)
    {
        ++tick;
        DBASE_LOG_INFO("[main] tick(restart) {} co_id1={} IsInCoroutine={}", tick, id1, sched.IsInCoroutine());
        sched.Resume(id1);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    DBASE_LOG_INFO("[main] destroy coroutines co_id1={} co_id2={}", id1, id2);
    sched.Destroy(id1);
    sched.Destroy(id2);

    DBASE_LOG_INFO("[main] exit");
    return 0;
}
