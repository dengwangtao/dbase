#include "dbase/thread/thread_pool.h"

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace dbase::thread
{
ThreadPool::ThreadPool(
        std::size_t threadCount,
        std::string threadNamePrefix,
        std::size_t queueCapacity)
    : m_threadCount(threadCount),
      m_queueCapacity(queueCapacity),
      m_threadNamePrefix(threadNamePrefix.empty() ? "worker" : std::move(threadNamePrefix)),
      m_tasks(queueCapacity)
{
    if (m_threadCount == 0)
    {
        throw std::invalid_argument("ThreadPool threadCount must be greater than 0");
    }

    m_workers.reserve(m_threadCount);
}

ThreadPool::~ThreadPool()
{
    stop();
}

void ThreadPool::start()
{
    bool expected = false;
    if (!m_started.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
    {
        throw std::logic_error("ThreadPool already started");
    }

    m_stopped.store(false, std::memory_order_release);

    for (std::size_t i = 0; i < m_threadCount; ++i)
    {
        const auto name = m_threadNamePrefix + "-" + std::to_string(i + 1);

        m_workers.emplace_back(
                [this, i]()
                {
                    workerLoop(i);
                },
                name);
    }

    try
    {
        for (auto& worker : m_workers)
        {
            worker.start();
        }
    }
    catch (...)
    {
        stop();
        throw;
    }
}

void ThreadPool::stop()
{
    bool expected = false;
    if (!m_stopped.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
    {
        return;
    }

    m_tasks.stop();

    for (auto& worker : m_workers)
    {
        if (worker.joinable())
        {
            worker.join();
        }
    }

    m_workers.clear();
}

bool ThreadPool::started() const noexcept
{
    return m_started.load(std::memory_order_acquire) && !m_stopped.load(std::memory_order_acquire);
}

bool ThreadPool::stopped() const noexcept
{
    return m_stopped.load(std::memory_order_acquire);
}

std::size_t ThreadPool::threadCount() const noexcept
{
    return m_threadCount;
}

std::size_t ThreadPool::queueCapacity() const noexcept
{
    return m_queueCapacity;
}

std::size_t ThreadPool::pendingTaskCount() const noexcept
{
    return m_tasks.size();
}

void ThreadPool::workerLoop(std::size_t)
{
    while (true)
    {
        try
        {
            std::optional<Task> task = m_tasks.popFor(200);
            if (!task.has_value())
            {
                continue;
            }

            task.value()();
        }
        catch (const std::runtime_error&)
        {
            break;
        }
    }
}

}  // namespace dbase::thread