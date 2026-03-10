#include "dbase/thread/serial_executor.h"

#include <optional>

namespace dbase::thread
{
SerialExecutor::SerialExecutor(ThreadPool& threadPool, std::size_t queueCapacity)
    : m_threadPool(threadPool),
      m_tasks(queueCapacity)
{
}

SerialExecutor::~SerialExecutor()
{
    stop();
}

void SerialExecutor::start()
{
    bool expected = false;
    if (!m_started.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
    {
        throw std::logic_error("SerialExecutor already started");
    }

    m_stopped.store(false, std::memory_order_release);
}

void SerialExecutor::stop()
{
    bool expected = false;
    if (!m_stopped.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
    {
        return;
    }

    m_tasks.stop();
}

bool SerialExecutor::started() const noexcept
{
    return m_started.load(std::memory_order_acquire) && !m_stopped.load(std::memory_order_acquire);
}

bool SerialExecutor::stopped() const noexcept
{
    return m_stopped.load(std::memory_order_acquire);
}

std::size_t SerialExecutor::pendingTaskCount() const noexcept
{
    return m_tasks.size();
}

std::size_t SerialExecutor::queueCapacity() const noexcept
{
    return m_tasks.capacity();
}

void SerialExecutor::schedule()
{
    bool expected = false;
    if (!m_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
    {
        return;
    }

    try
    {
        m_threadPool.submit([this]()
                            { drain(); });
    }
    catch (...)
    {
        m_running.store(false, std::memory_order_release);
        throw;
    }
}

void SerialExecutor::drain()
{
    for (;;)
    {
        std::optional<Task> task = m_tasks.tryPop();
        if (!task.has_value())
        {
            break;
        }

        task.value()();
    }

    m_running.store(false, std::memory_order_release);

    if (!m_tasks.empty() && !m_stopped.load(std::memory_order_acquire))
    {
        schedule();
    }
}

}  // namespace dbase::thread