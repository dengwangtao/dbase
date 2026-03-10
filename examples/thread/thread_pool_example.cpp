#include "dbase/log/log.h"
#include "dbase/thread/current_thread.h"
#include "dbase/thread/thread_pool.h"

#include <cstdlib>
#include <string>
#include <vector>
#include <random>

int main()
{
    dbase::log::setDefaultLevel(dbase::log::Level::Trace);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::Source);

    dbase::thread::ThreadPool pool(4, "pool", 128);
    pool.start();

    auto future1 = pool.submit([]()
                               {
        dbase::thread::current_thread::sleepForMs(1000);
        DBASE_LOG_INFO("task-1 running, tid={}", dbase::thread::current_thread::tid());
        return 1 + 2; });

    auto future2 = pool.submit([](int a, int b)
                               {
        DBASE_LOG_INFO("task-2 running, tid={}", dbase::thread::current_thread::tid());
        return a + b; }, 10, 20);

    std::vector<std::future<std::string>> futures;
    for (int i = 0; i < 10; ++i)
    {
        futures.emplace_back(
                pool.submit([i]()
                            {
                dbase::thread::current_thread::sleepForMs(std::random_device{}() % 1000 + 1000);
                DBASE_LOG_INFO("task-{} running, tid={} tname={}", i, dbase::thread::current_thread::tid(), dbase::thread::current_thread::name());
                return std::string("done-") + std::to_string(i); }));
    }

    DBASE_LOG_INFO("future1={}", future1.get());
    DBASE_LOG_INFO("future2={}", future2.get());

    for (auto& future : futures)
    {
        DBASE_LOG_INFO("result={}", future.get());
    }

    pool.stop();
    DBASE_LOG_INFO("thread pool stopped");
    return 0;
}