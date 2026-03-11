#include "dbase/log/log.h"
#include "dbase/net/event_loop.h"
#include "dbase/net/event_loop_thread_pool.h"
#include "dbase/net/socket_ops.h"
#include "dbase/thread/current_thread.h"
#include "dbase/thread/thread.h"

int main()
{
    dbase::log::setDefaultLevel(dbase::log::Level::Trace);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::Source);

    const auto initRet = dbase::net::SocketOps::initialize();
    if (!initRet)
    {
        DBASE_LOG_ERROR("socket initialize failed: {}", initRet.error().message());
        return 1;
    }

    try
    {
        dbase::net::EventLoop baseLoop;
        dbase::net::EventLoopThreadPool pool(&baseLoop, "io-loop", 3);

        dbase::thread::Thread producer(
                [&baseLoop, &pool](std::stop_token)
                {
                    dbase::thread::current_thread::sleepForMs(500);

                    baseLoop.runInLoop([&baseLoop, &pool]()
                                       {
                    DBASE_LOG_INFO("start event loop thread pool");
                    pool.start();

                    for (int i = 0; i < 9; ++i)
                    {
                        auto* loop = pool.getNextLoop();
                        loop->queueInLoop([i, loop]() {
                            DBASE_LOG_INFO(
                                "task {} running in sub loop thread, loopPtr={}",
                                i,
                                reinterpret_cast<std::uintptr_t>(loop));
                        });
                    }

                    for (std::size_t i = 0; i < pool.loops().size(); ++i)
                    {
                        auto* loop = pool.loops()[i];
                        loop->queueInLoop([i]() {
                            DBASE_LOG_INFO("direct queue to loop index={}", i);
                        });
                    }

                    baseLoop.queueInLoop([&baseLoop, &pool]() {
                        
                        dbase::thread::current_thread::sleepForMs(1000);

                        for (auto* loop : pool.loops())
                        {
                            loop->quit();
                        }
                        baseLoop.quit();
                    }); });
                },
                "pool-producer");

        producer.start();

        DBASE_LOG_INFO("base loop start");
        baseLoop.loop();
        DBASE_LOG_INFO("base loop exit");

        producer.join();
    }
    catch (const std::exception& ex)
    {
        DBASE_LOG_ERROR("exception: {}", ex.what());
        dbase::net::SocketOps::cleanup();
        return 1;
    }

    dbase::net::SocketOps::cleanup();
    return 0;
}