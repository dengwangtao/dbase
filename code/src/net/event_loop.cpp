#include "dbase/net/event_loop.h"

#include "dbase/net/channel.h"
#include "dbase/net/epoll_poller.h"
#include "dbase/net/select_poller.h"
#include "dbase/net/wakeup_channel.h"
#include "dbase/thread/current_thread.h"

#include <algorithm>
#include <limits>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace dbase::net
{
Poller::Poller(EventLoop* loop)
    : m_ownerLoop(loop)
{
}

EventLoop::EventLoop()
    : m_threadId(dbase::thread::current_thread::tid()),
#if defined(__linux__)
      m_poller(std::make_unique<EpollPoller>(this)),
#else
      m_poller(std::make_unique<SelectPoller>(this)),
#endif
      m_wakeupChannel(std::make_unique<WakeupChannel>()),
      m_wakeupFdChannel(std::make_unique<Channel>(this, m_wakeupChannel->readFd()))
{
    m_wakeupFdChannel->setReadCallback([this]()
                                       { handleWakeupRead(); });
    m_wakeupFdChannel->enableReading();
}

EventLoop::~EventLoop()
{
    m_quit.store(true, std::memory_order_release);

    if (m_wakeupFdChannel)
    {
        m_wakeupFdChannel->disableAll();
        m_wakeupFdChannel->remove();
    }
}

void EventLoop::loop()
{
    assertInLoopThread();

    if (m_looping.exchange(true, std::memory_order_acq_rel))
    {
        throw std::logic_error("EventLoop already looping");
    }

    m_quit.store(false, std::memory_order_release);

    while (!m_quit.load(std::memory_order_acquire))
    {
        processTimers();

        m_activeChannels.clear();
        m_poller->poll(getPollTimeoutMs(), &m_activeChannels);

        for (auto* channel : m_activeChannels)
        {
            channel->handleEvent();
        }

        doPendingFunctors();
        processTimers();
    }

    m_looping.store(false, std::memory_order_release);
}

void EventLoop::quit() noexcept
{
    m_quit.store(true, std::memory_order_release);

    if (!isInLoopThread())
    {
        wakeup();
    }
}

bool EventLoop::looping() const noexcept
{
    return m_looping.load(std::memory_order_acquire);
}

bool EventLoop::quitRequested() const noexcept
{
    return m_quit.load(std::memory_order_acquire);
}

std::uint64_t EventLoop::threadId() const noexcept
{
    return m_threadId;
}

void EventLoop::assertInLoopThread() const
{
    if (!isInLoopThread())
    {
        throw std::runtime_error("EventLoop used from another thread");
    }
}

bool EventLoop::isInLoopThread() const noexcept
{
    return m_threadId == dbase::thread::current_thread::tid();
}

void EventLoop::runInLoop(Functor cb)
{
    if (isInLoopThread())
    {
        cb();
        return;
    }

    queueInLoop(std::move(cb));
}

void EventLoop::queueInLoop(Functor cb)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_pendingFunctors.emplace_back(std::move(cb));
    }

    if (!isInLoopThread() || m_callingPendingFunctors.load(std::memory_order_acquire))
    {
        wakeup();
    }
}

EventLoop::TimerId EventLoop::runAfter(std::chrono::milliseconds delay, Functor cb)
{
    if (delay.count() < 0)
    {
        delay = std::chrono::milliseconds(0);
    }

    return runAt(Clock::now() + delay, std::chrono::milliseconds(0), false, std::move(cb));
}

EventLoop::TimerId EventLoop::runEvery(std::chrono::milliseconds interval, Functor cb)
{
    if (interval.count() <= 0)
    {
        throw std::invalid_argument("EventLoop::runEvery interval must be greater than 0");
    }

    return runAt(Clock::now() + interval, interval, true, std::move(cb));
}

void EventLoop::cancelTimer(TimerId timerId)
{
    if (timerId == 0)
    {
        return;
    }

    if (isInLoopThread())
    {
        cancelTimerInLoop(timerId);
        return;
    }

    queueInLoop([this, timerId]()
                { cancelTimerInLoop(timerId); });
}

void EventLoop::updateChannel(Channel* channel)
{
    assertInLoopThread();
    m_poller->updateChannel(channel);
}

void EventLoop::removeChannel(Channel* channel)
{
    assertInLoopThread();
    m_poller->removeChannel(channel);
}

Poller* EventLoop::poller() noexcept
{
    return m_poller.get();
}

const std::vector<Channel*>& EventLoop::activeChannels() const noexcept
{
    return m_activeChannels;
}

void EventLoop::wakeup()
{
    m_wakeupChannel->wakeup();
}

void EventLoop::handleWakeupRead()
{
    m_wakeupChannel->handleRead();
}

void EventLoop::doPendingFunctors()
{
    std::vector<Functor> functors;
    m_callingPendingFunctors.store(true, std::memory_order_release);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        functors.swap(m_pendingFunctors);
    }

    for (auto& functor : functors)
    {
        functor();
    }

    m_callingPendingFunctors.store(false, std::memory_order_release);
}

EventLoop::TimerId EventLoop::runAt(TimePoint expiration, std::chrono::milliseconds interval, bool repeat, Functor cb)
{
    if (!cb)
    {
        throw std::invalid_argument("EventLoop timer callback is empty");
    }

    auto timerTask = std::make_shared<TimerTask>();
    timerTask->id = m_nextTimerId.fetch_add(1, std::memory_order_relaxed);
    timerTask->expiration = expiration;
    timerTask->interval = interval;
    timerTask->repeat = repeat;
    timerTask->callback = std::move(cb);

    if (isInLoopThread())
    {
        addTimerInLoop(timerTask);
    }
    else
    {
        queueInLoop([this, timerTask]()
                    { addTimerInLoop(timerTask); });
    }

    return timerTask->id;
}

void EventLoop::addTimerInLoop(const std::shared_ptr<TimerTask>& timerTask)
{
    assertInLoopThread();

    bool wake = m_timers.empty() || timerTask->expiration < m_timers.top()->expiration;

    m_timerMap[timerTask->id] = timerTask;
    m_timers.push(timerTask);

    if (wake)
    {
        wakeup();
    }
}

void EventLoop::cancelTimerInLoop(TimerId timerId)
{
    assertInLoopThread();

    const auto it = m_timerMap.find(timerId);
    if (it == m_timerMap.end())
    {
        return;
    }

    it->second->cancelled = true;
    m_timerMap.erase(it);
}

void EventLoop::processTimers()
{
    assertInLoopThread();

    const auto now = Clock::now();

    while (!m_timers.empty())
    {
        auto timerTask = m_timers.top();

        if (timerTask->cancelled)
        {
            m_timers.pop();
            continue;
        }

        if (timerTask->expiration > now)
        {
            break;
        }

        m_timers.pop();

        const auto it = m_timerMap.find(timerTask->id);
        if (it == m_timerMap.end())
        {
            continue;
        }

        if (!timerTask->cancelled && timerTask->callback)
        {
            timerTask->callback();
        }

        if (timerTask->repeat && !timerTask->cancelled)
        {
            timerTask->expiration = Clock::now() + timerTask->interval;
            m_timers.push(timerTask);
        }
        else
        {
            m_timerMap.erase(timerTask->id);
        }
    }
}

int EventLoop::getPollTimeoutMs() const noexcept
{
    if (m_timers.empty())
    {
        return 10000;
    }

    const auto now = Clock::now();
    const auto& timerTask = m_timers.top();

    if (timerTask->cancelled)
    {
        return 0;
    }

    if (timerTask->expiration <= now)
    {
        return 0;
    }

    const auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(timerTask->expiration - now).count();

    if (diff <= 0)
    {
        return 0;
    }

    if (diff > std::numeric_limits<int>::max())
    {
        return std::numeric_limits<int>::max();
    }

    return static_cast<int>(diff);
}

}  // namespace dbase::net