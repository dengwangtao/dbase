#include "dbase/log/log.h"
#include "dbase/thread/current_thread.h"
#include "dbase/thread/serial_executor.h"
#include "dbase/thread/thread_pool.h"

#include <future>
#include <string>
#include <vector>

int main()
{
    dbase::log::setDefaultLevel(dbase::log::Level::Trace);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::Source);

    dbase::thread::ThreadPool pool(4, "pool", 256);
    pool.start();

    dbase::thread::SerialExecutor executor(pool, 128);
    executor.start();

    std::vector<std::future<std::string>> futures;

    for (int i = 0; i < 10; ++i)
    {
        futures.emplace_back(
                executor.submit([i]()
                                {
                DBASE_LOG_INFO("serial task begin, index={}, tid={}",
                               i,
                               dbase::thread::current_thread::tid());

                dbase::thread::current_thread::sleepForMs(50);

                DBASE_LOG_INFO("serial task end, index={}, tid={}",
                               i,
                               dbase::thread::current_thread::tid());

                return std::string("done-") + std::to_string(i); }));
    }

    for (auto& future : futures)
    {
        DBASE_LOG_INFO("result={}", future.get());
    }

    executor.stop();
    pool.stop();

    DBASE_LOG_INFO("serial executor stopped");
    return 0;
}