#include "dbase/log/log.h"
#include "dbase/thread/current_thread.h"
#include "dbase/thread/thread_pool.h"
#include "dbase/thread/timer_queue.h"

#include <chrono>

int main()
{
    dbase::log::setDefaultLevel(dbase::log::Level::Trace);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::Source);

    dbase::thread::ThreadPool pool(2, "timer-worker", 128);
    pool.start();

    dbase::thread::TimerQueue timerQueue(&pool, "timer-loop");
    timerQueue.start();

    (void)timerQueue.runAfter(std::chrono::milliseconds(500), []()
                        { DBASE_LOG_INFO("runAfter fired, tid={}", dbase::thread::current_thread::tid()); });

    (void)timerQueue.runAt(dbase::thread::TimerQueue::Clock::now() + std::chrono::seconds(1), []()
                     { DBASE_LOG_INFO("runAt fired, tid={}", dbase::thread::current_thread::tid()); });

    const auto repeatId = timerQueue.runEvery(std::chrono::milliseconds(1000), []()
                                              { DBASE_LOG_INFO("runEvery fired, tid={}", dbase::thread::current_thread::tid()); });

    dbase::thread::current_thread::sleepForMs(3100);
    timerQueue.cancel(repeatId);

    DBASE_LOG_INFO("repeat timer cancelled");

    dbase::thread::current_thread::sleepForMs(1000);

    timerQueue.stop();
    pool.stop();

    DBASE_LOG_INFO("timer queue stopped");
    return 0;
}