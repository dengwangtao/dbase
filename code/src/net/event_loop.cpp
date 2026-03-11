#include "dbase/net/event_loop.h"

#include "dbase/net/channel.h"
#include "dbase/net/select_poller.h"
#include "dbase/net/wakeup_channel.h"
#include "dbase/thread/current_thread.h"

#include <stdexcept>
#include <utility>

namespace dbase::net
{
Poller::Poller(EventLoop* loop)
    : m_ownerLoop(loop)
{
}

EventLoop::EventLoop()
    : m_threadId(dbase::thread::current_thread::tid()),
      m_poller(std::make_unique<SelectPoller>(this)),
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
        m_activeChannels.clear();
        m_poller->poll(1000, &m_activeChannels);

        for (auto* channel : m_activeChannels)
        {
            channel->handleEvent();
        }

        doPendingFunctors();
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

}  // namespace dbase::net