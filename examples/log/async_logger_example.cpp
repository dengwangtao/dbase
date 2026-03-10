#include "dbase/log/async_logger.h"
#include "dbase/log/sink.h"

#include <memory>
#include <source_location>
#include <thread>
#include <vector>

int main()
{
    dbase::log::AsyncLogger logger(
            dbase::log::PatternStyle::Threaded,
            4096,
            dbase::log::AsyncOverflowPolicy::Block);

    logger.clearSinks();
    logger.addSink(std::make_shared<dbase::log::ConsoleSink>());
    logger.addSink(std::make_shared<dbase::log::RotatingFileSink>("logs/async.log", 1024 * 64, 3));

    logger.setLevel(dbase::log::Level::Trace);
    logger.setFlushOn(dbase::log::Level::Error);

    std::vector<std::thread> threads;

    for (int t = 0; t < 4; ++t)
    {
        threads.emplace_back([&logger, t]()
                             {
            for (int i = 0; i < 1000; ++i)
            {
                logger.logf(
                    dbase::log::Level::Info,
                    std::source_location::current(),
                    "thread={}, index={}",
                    t,
                    i);
            }

            logger.logf(
                dbase::log::Level::Error,
                std::source_location::current(),
                "thread={} done with error-style flush point",
                t); });
    }

    for (auto& thread : threads)
    {
        thread.join();
    }

    logger.flush();
    logger.stop();
    return 0;
}