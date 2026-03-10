#include "dbase/log/log.h"
#include "dbase/thread/current_thread.h"
#include "dbase/thread/sharded_executor.h"
#include "dbase/thread/thread_pool.h"

#include <future>
#include <string>
#include <vector>

int main()
{
    dbase::log::setDefaultLevel(dbase::log::Level::Trace);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::Source);

    dbase::thread::ThreadPool pool(4, "pool", 512);
    pool.start();

    dbase::thread::ShardedExecutor executor(pool, 4, 128);
    executor.start();

    std::vector<std::future<std::string>> futures;

    for (int i = 0; i < 20; ++i)
    {
        const int playerId = i % 3;

        futures.emplace_back(
                executor.submit(playerId, [i, playerId]()
                                {
                DBASE_LOG_INFO("task begin, playerId={}, index={}, tid={}",
                               playerId,
                               i,
                               dbase::thread::current_thread::tid());

                dbase::thread::current_thread::sleepForMs(50);

                DBASE_LOG_INFO("task end, playerId={}, index={}, tid={}",
                               playerId,
                               i,
                               dbase::thread::current_thread::tid());

                return std::string("done-player-") +
                       std::to_string(playerId) +
                       "-task-" +
                       std::to_string(i); }));
    }

    for (auto& future : futures)
    {
        DBASE_LOG_INFO("result={}", future.get());
    }

    DBASE_LOG_INFO("total pending={}", executor.pendingTaskCount());

    executor.stop();
    pool.stop();

    DBASE_LOG_INFO("sharded executor stopped");
    return 0;
}