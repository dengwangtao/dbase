#include "dbase/net/event_loop_thread_pool.h"

#include <stdexcept>

namespace dbase::net
{
EventLoopThreadPool::EventLoopThreadPool(
        EventLoop* baseLoop,
        std::string poolName,
        std::size_t threadCount)
    : m_baseLoop(baseLoop),
      m_poolName(poolName.empty() ? "event-loop-pool" : std::move(poolName)),
      m_threadCount(threadCount)
{
    if (m_baseLoop == nullptr)
    {
        throw std::invalid_argument("EventLoopThreadPool baseLoop is null");
    }
}

EventLoopThreadPool::~EventLoopThreadPool() = default;

void EventLoopThreadPool::start()
{
    if (m_started)
    {
        throw std::logic_error("EventLoopThreadPool already started");
    }

    m_baseLoop->assertInLoopThread();

    m_started = true;
    m_threads.reserve(m_threadCount);
    m_loops.reserve(m_threadCount);

    for (std::size_t i = 0; i < m_threadCount; ++i)
    {
        const auto threadName = m_poolName + "-" + std::to_string(i + 1);
        auto thread = std::make_unique<EventLoopThread>(threadName);
        auto* loop = thread->startLoop();
        m_loops.emplace_back(loop);
        m_threads.emplace_back(std::move(thread));
    }
}

bool EventLoopThreadPool::started() const noexcept
{
    return m_started;
}

EventLoop* EventLoopThreadPool::baseLoop() const noexcept
{
    return m_baseLoop;
}

const std::string& EventLoopThreadPool::poolName() const noexcept
{
    return m_poolName;
}

std::size_t EventLoopThreadPool::threadCount() const noexcept
{
    return m_threadCount;
}

std::size_t EventLoopThreadPool::loopCount() const noexcept
{
    return m_loops.size();
}

EventLoop* EventLoopThreadPool::getNextLoop()
{
    m_baseLoop->assertInLoopThread();

    if (m_loops.empty())
    {
        return m_baseLoop;
    }

    EventLoop* loop = m_loops[m_next];
    ++m_next;
    if (m_next >= m_loops.size())
    {
        m_next = 0;
    }

    return loop;
}

EventLoop* EventLoopThreadPool::getLoopForHash(std::size_t hashCode)
{
    m_baseLoop->assertInLoopThread();

    if (m_loops.empty())
    {
        return m_baseLoop;
    }

    return m_loops[hashCode % m_loops.size()];
}

const std::vector<EventLoop*>& EventLoopThreadPool::loops() const noexcept
{
    return m_loops;
}

}  // namespace dbase::net