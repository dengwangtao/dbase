#include "dbase/log/log.h"
#include "dbase/sync/blocking_queue.h"
#include "dbase/sync/count_down_latch.h"
#include "dbase/thread/current_thread.h"
#include "dbase/thread/thread.h"

#include <optional>
#include <string>
#include <vector>

int main()
{
    dbase::log::setDefaultLevel(dbase::log::Level::Trace);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::Source);

    dbase::sync::CountDownLatch readyLatch(3);
    dbase::sync::BlockingQueue<std::string> queue(16);

    std::vector<dbase::thread::Thread> workers;
    workers.emplace_back(
            [&readyLatch, &queue](std::stop_token stopToken)
            {
                dbase::thread::current_thread::setName("worker-1");
                readyLatch.countDown();

                while (!stopToken.stop_requested())
                {
                    const auto item = queue.popFor(200);
                    if (!item.has_value())
                    {
                        continue;
                    }

                    DBASE_LOG_INFO("thread={}, tid={}, item={}",
                                   dbase::thread::current_thread::name(),
                                   dbase::thread::current_thread::tid(),
                                   item.value());
                }
            },
            "worker-1");

    workers.emplace_back(
            [&readyLatch, &queue](std::stop_token stopToken)
            {
                dbase::thread::current_thread::setName("worker-2");
                readyLatch.countDown();

                while (!stopToken.stop_requested())
                {
                    const auto item = queue.popFor(200);
                    if (!item.has_value())
                    {
                        continue;
                    }

                    DBASE_LOG_INFO("thread={}, tid={}, item={}",
                                   dbase::thread::current_thread::name(),
                                   dbase::thread::current_thread::tid(),
                                   item.value());
                }
            },
            "worker-2");

    workers.emplace_back(
            [&readyLatch, &queue](std::stop_token stopToken)
            {
                dbase::thread::current_thread::setName("worker-3");
                readyLatch.countDown();

                while (!stopToken.stop_requested())
                {
                    const auto item = queue.popFor(200);
                    if (!item.has_value())
                    {
                        continue;
                    }

                    DBASE_LOG_INFO("thread={}, tid={}, item={}",
                                   dbase::thread::current_thread::name(),
                                   dbase::thread::current_thread::tid(),
                                   item.value());
                }
            },
            "worker-3");

    for (auto& worker : workers)
    {
        worker.start();
    }

    readyLatch.wait();

    for (int i = 0; i < 20; ++i)
    {
        queue.push("task-" + std::to_string(i));
    }

    dbase::thread::current_thread::sleepForMs(1000);

    queue.stop();

    for (auto& worker : workers)
    {
        worker.requestStop();
        worker.join();
    }

    DBASE_LOG_INFO("main thread done");
    return 0;
}