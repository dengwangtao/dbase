#include "dbase/net/event_loop_thread.h"

#include <stdexcept>

namespace dbase::net
{
EventLoopThread::EventLoopThread(std::string threadName)
    : m_threadName(threadName.empty() ? "event-loop" : std::move(threadName)),
      m_thread(
              [this](std::stop_token stopToken)
              {
                  threadFunc(stopToken);
              },
              m_threadName)
{
}

EventLoopThread::~EventLoopThread()
{
    stop();
}

EventLoop* EventLoopThread::startLoop()
{
    if (m_started)
    {
        throw std::logic_error("EventLoopThread already started");
    }

    m_started = true;
    m_thread.start();
    m_latch.wait();

    if (m_loop == nullptr)
    {
        throw std::runtime_error("EventLoopThread failed to start loop");
    }

    return m_loop;
}

void EventLoopThread::stop()
{
    if (!m_started)
    {
        return;
    }

    if (m_loop != nullptr)
    {
        m_loop->quit();
    }

    if (m_thread.joinable())
    {
        m_thread.join();
    }

    m_loop = nullptr;
    m_started = false;
}

bool EventLoopThread::started() const noexcept
{
    return m_started;
}

const std::string& EventLoopThread::threadName() const noexcept
{
    return m_threadName;
}

void EventLoopThread::threadFunc(std::stop_token)
{
    EventLoop loop;
    m_loop = &loop;
    m_latch.countDown();
    loop.loop();
    m_loop = nullptr;
}

}  // namespace dbase::net