#include "dbase/thread/thread.h"

#include "dbase/thread/current_thread.h"

#include <stdexcept>
#include <utility>

namespace dbase::thread
{
Thread::Thread(StopFunction func, std::string name)
    : m_func(std::move(func)),
      m_name(name.empty() ? "thread" : std::move(name))
{
}

Thread::Thread(Function func, std::string name)
    : Thread(
              [func = std::move(func)](std::stop_token)
              {
                  func();
              },
              std::move(name))
{
}

Thread::Thread(Thread&& other) noexcept
    : m_func(std::move(other.m_func)),
      m_name(std::move(other.m_name)),
      m_thread(std::move(other.m_thread)),
      m_started(other.m_started.load(std::memory_order_acquire)),
      m_stopRequested(other.m_stopRequested.load(std::memory_order_acquire)),
      m_tid(other.m_tid.load(std::memory_order_acquire))
{
    other.m_started.store(false, std::memory_order_release);
    other.m_stopRequested.store(false, std::memory_order_release);
    other.m_tid.store(0, std::memory_order_release);
    if (other.m_name.empty())
    {
        other.m_name = "thread";
    }
}

Thread& Thread::operator=(Thread&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }

    if (m_thread.joinable())
    {
        m_thread.request_stop();
        m_stopRequested.store(true, std::memory_order_release);
        m_thread.join();
    }

    m_func = std::move(other.m_func);
    m_name = std::move(other.m_name);
    m_thread = std::move(other.m_thread);
    m_started.store(other.m_started.load(std::memory_order_acquire), std::memory_order_release);
    m_stopRequested.store(other.m_stopRequested.load(std::memory_order_acquire), std::memory_order_release);
    m_tid.store(other.m_tid.load(std::memory_order_acquire), std::memory_order_release);

    other.m_started.store(false, std::memory_order_release);
    other.m_stopRequested.store(false, std::memory_order_release);
    other.m_tid.store(0, std::memory_order_release);
    if (other.m_name.empty())
    {
        other.m_name = "thread";
    }

    return *this;
}

Thread::~Thread()
{
    if (m_thread.joinable())
    {
        m_thread.request_stop();
        m_stopRequested.store(true, std::memory_order_release);
        m_thread.join();
    }
}

void Thread::start()
{
    if (m_started.exchange(true, std::memory_order_acq_rel))
    {
        throw std::logic_error("Thread already started");
    }

    if (!m_func)
    {
        throw std::logic_error("Thread function is empty");
    }

    auto func = m_func;
    auto name = m_name;

    m_thread = std::jthread(
            [this, func = std::move(func), name = std::move(name)](std::stop_token stopToken) mutable
            {
                current_thread::setName(name);
                m_tid.store(current_thread::tid(), std::memory_order_release);
                func(stopToken);
            });
}

void Thread::join()
{
    if (m_thread.joinable())
    {
        m_thread.join();
    }
}

void Thread::detach()
{
    if (m_thread.joinable())
    {
        m_thread.detach();
    }
}

void Thread::requestStop()
{
    if (m_thread.joinable())
    {
        m_stopRequested.store(true, std::memory_order_release);
        m_thread.request_stop();
    }
}

bool Thread::joinable() const noexcept
{
    return m_thread.joinable();
}

bool Thread::started() const noexcept
{
    return m_started.load(std::memory_order_acquire);
}

bool Thread::stopRequested() const noexcept
{
    if (m_stopRequested.load(std::memory_order_acquire))
    {
        return true;
    }

    if (m_thread.joinable())
    {
        return m_thread.get_stop_token().stop_requested();
    }

    return false;
}

std::uint64_t Thread::tid() const noexcept
{
    return m_tid.load(std::memory_order_acquire);
}

const std::string& Thread::name() const noexcept
{
    return m_name;
}

void Thread::setName(std::string name)
{
    if (started())
    {
        throw std::logic_error("Cannot rename a started thread");
    }

    if (!name.empty())
    {
        m_name = std::move(name);
    }
}

}  // namespace dbase::thread