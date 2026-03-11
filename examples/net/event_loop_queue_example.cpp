#include "dbase/log/log.h"
#include "dbase/net/event_loop.h"
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
        dbase::net::EventLoop loop;

        dbase::thread::Thread producer(
            [&loop](std::stop_token) {
                dbase::thread::current_thread::sleepForMs(500);

                loop.queueInLoop([]() {
                    DBASE_LOG_INFO("queueInLoop task-1 running in loop thread");
                });

                loop.runInLoop([]() {
                    DBASE_LOG_INFO("runInLoop task-2 running in loop thread");
                });

                loop.queueInLoop([&loop]() {
                    DBASE_LOG_INFO("queueInLoop task-3 quit loop");
                    loop.quit();
                });
            },
            "loop-producer");

        producer.start();

        DBASE_LOG_INFO("event loop start");
        loop.loop();
        DBASE_LOG_INFO("event loop exit");

        producer.join();
        dbase::net::SocketOps::cleanup();
        return 0;
    }
    catch (const std::exception& ex)
    {
        DBASE_LOG_ERROR("exception: {}", ex.what());
        dbase::net::SocketOps::cleanup();
        return 1;
    }
}