#include "dbase/time/timer_wheel.h"
#include "dbase/time/time.h"
#include <utility>

namespace dbase::time
{

TimerMgr::Wheel::Wheel(uint32_t no)
    : wheel_no(no),
      tlist(ELEMENT_CNT_PER_WHEEL[no]),
      cur_slot(0)
{
}

void TimerMgr::Wheel::clear() noexcept
{
    for (DoubleLink& list : tlist)
    {
        list.Clear();
    }
}

uint32_t TimerMgr::Wheel::slotCount() const noexcept
{
    return ELEMENT_CNT_PER_WHEEL[wheel_no];
}

TimerMgr::TimerMgr()
    : wheels_{Wheel(0), Wheel(1), Wheel(2), Wheel(3), Wheel(4)},
      is_ms_timer_(false),
      last_tick_(0)
{
}

int32_t TimerMgr::registerTimer(Timer* timer, uint32_t timeout)
{
    if (timer == nullptr)
    {
        return -1;
    }

    if (timer->isRegistered())
    {
        unRegisterTimer(timer);
    }

    if (last_tick_ > 0)
    {
        const uint64_t cur_tick = getNow();
        if (cur_tick > last_tick_)
        {
            timeout = static_cast<uint32_t>((cur_tick - last_tick_) + timeout);
        }
    }

    // 0 tick 定时器在下一次 step 执行
    timeout = (timeout == 0) ? 1u : timeout;

    uint32_t wheel_no = 0;
    uint32_t offset = timeout;
    uint32_t left = timeout;

    while (wheel_no + 1 < WHEEL_CNT && offset >= ELEMENT_CNT_PER_WHEEL[wheel_no])
    {
        const Wheel& wheel = wheels_[wheel_no];
        offset >>= RIGHT_SHIFT_PER_WHEEL[wheel_no];
        left -= BASE_TICK_PER_WHEEL[wheel_no] * (ELEMENT_CNT_PER_WHEEL[wheel_no] - wheel.cur_slot - (wheel_no == 0 ? 0u : 1u));
        ++wheel_no;
    }

    left -= BASE_TICK_PER_WHEEL[wheel_no] * (offset - 1u);

    const uint32_t slot =
            (offset + wheels_[wheel_no].cur_slot) % ELEMENT_CNT_PER_WHEEL[wheel_no];

    timer->setTimeOut(left);
    wheels_[wheel_no].tlist[slot].Push(timer);
    return 0;
}

int32_t TimerMgr::unRegisterTimer(Timer* timer) noexcept
{
    if (timer == nullptr || !timer->isRegistered())
    {
        return -1;
    }

    DoubleLink::Remove(timer);
    return 0;
}

void TimerMgr::initTick()
{
    is_ms_timer_ = false;
    last_tick_ = nowS();
    resetWheels();
}

void TimerMgr::tick()
{
    tickInternal(nowS());
}

void TimerMgr::initTickMs()
{
    is_ms_timer_ = true;
    last_tick_ = nowMs();
    resetWheels();
}

void TimerMgr::tickMs()
{
    tickInternal(nowMs());
}

void TimerMgr::clear() noexcept
{
    for (Wheel& wheel : wheels_)
    {
        wheel.clear();
        wheel.cur_slot = 0;
    }
}

void TimerMgr::tickInternal(uint64_t cur_tick)
{
    if (cur_tick <= last_tick_)
    {
        return;
    }

    uint64_t count = cur_tick - last_tick_;
    while (count > 0)
    {
        ++last_tick_;
        step();
        --count;
    }
}

uint64_t TimerMgr::getNow() const noexcept
{
    return is_ms_timer_ ? nowMs() : nowS();
}

void TimerMgr::resetWheels() noexcept
{
    for (Wheel& wheel : wheels_)
    {
        wheel.cur_slot = 0;
    }
}

void TimerMgr::step()
{
    for (uint32_t wheel_no = 0; wheel_no < WHEEL_CNT; ++wheel_no)
    {
        Wheel& wheel = wheels_[wheel_no];
        wheel.cur_slot = (wheel.cur_slot + 1u) % ELEMENT_CNT_PER_WHEEL[wheel_no];

        DoubleLink& cur_list = wheel.tlist[wheel.cur_slot];
        while (!cur_list.Empty())
        {
            Timer* timer = static_cast<Timer*>(cur_list.Pop());
            if (timer == nullptr)
            {
                continue;
            }

            if (wheel_no == 0 || timer->getTimeOut() == 0)
            {
                timer->onTimeOut();
            }
            else
            {
                registerTimer(timer, timer->getTimeOut());
            }
        }

        if (wheel.cur_slot != 0)
        {
            break;
        }
    }
}
}  // namespace dbase::time
