#include <iostream>
#include <string>

#include "dbase/log/log.h"
#include "dbase/time/time.h"
#include "dbase/time/timer_wheel.h"
#include "dbase/thread/current_thread.h"

using namespace dbase::time;

int64_t timer_count = 0;

class PrintTimer final : public Timer
{
    public:
        explicit PrintTimer(std::string name)
            : name_(std::move(name))
        {
        }

        void onTimeOut() override
        {
            -- timer_count;
            DBASE_LOG_INFO("timeout: {} at tick={}", name_, formatNow());
        }

    private:
        std::string name_;
};

int main()
{
    dbase::log::setDefaultLevel(dbase::log::Level::Trace);
    dbase::log::setDefaultPatternStyle(dbase::log::PatternStyle::Source);

    TimerMgr mgr;
    mgr.initTick();

    PrintTimer timer_a("A");
    PrintTimer timer_b("B");
    PrintTimer timer_c("C");
    PrintTimer timer_d("D");
    PrintTimer timer_e("E");
    timer_count = 5;

    mgr.registerTimer(&timer_a, 1);
    mgr.registerTimer(&timer_b, 3);
    mgr.registerTimer(&timer_c, 5);
    mgr.registerTimer(&timer_d, 0);
    mgr.registerTimer(&timer_e, 10);

    while (timer_count > 0)
    {
        DBASE_LOG_INFO("tick -> {}", formatNow());
        mgr.tick();

        dbase::thread::current_thread::sleepForMs(500);
    }

    return 0;
}