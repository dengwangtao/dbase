#include "dbase/thread/timer_queue.h"

#include "dbase/thread/thread_pool.h"

#include <stdexcept>
#include <utility>

namespace dbase::thread
{
TimerQueue::TimerQueue(ThreadPool* threadPool, std::string threadName)
    : m_threadPool(threadPool),
      m_threadName(threadName.empty() ? "timer" : std::move(threadName))
{
}

TimerQueue::~TimerQueue()
{
    stop();
}

void TimerQueue::start()
{
    bool expected = false;
    if (!m_started.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
    {
        throw std::logic_error("TimerQueue already started");
    }

    m_stopped.store(false, std::memory_order_release);

    m_thread = dbase::thread::Thread(
            [this](std::stop_token stopToken)
            {
                workerLoop(stopToken);
            },
            m_threadName);

    try
    {
        m_thread.start();
    }
    catch (...)
    {
        m_stopped.store(true, std::memory_order_release);
        throw;
    }
}

void TimerQueue::stop()
{
    bool expected = false;
    if (!m_stopped.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
    {
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_cancelGeneration;
        while (!m_tasks.empty())
        {
            m_tasks.pop();
        }
        m_cancelled.clear();
    }

    m_cv.notify_all();

    m_thread.requestStop();
    if (m_thread.joinable())
    {
        m_thread.join();
    }
}

bool TimerQueue::started() const noexcept
{
    return m_started.load(std::memory_order_acquire) && !m_stopped.load(std::memory_order_acquire);
}

bool TimerQueue::stopped() const noexcept
{
    return m_stopped.load(std::memory_order_acquire);
}

void TimerQueue::setThreadPool(ThreadPool* threadPool)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_threadPool = threadPool;
}

TimerQueue::TimerId TimerQueue::runAfter(Duration delay, Task task)
{
    return runAt(Clock::now() + delay, std::move(task));
}

TimerQueue::TimerId TimerQueue::runAt(TimePoint timePoint, Task task)
{
    if (!started())
    {
        throw std::logic_error("TimerQueue is not started");
    }

    if (!task)
    {
        throw std::invalid_argument("TimerQueue task is empty");
    }

    auto timerTask = std::make_shared<TimerTask>();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        timerTask->id = ++m_nextId;
        timerTask->expiration = timePoint;
        timerTask->task = std::move(task);
        timerTask->repeat = false;
        timerTask->interval = Duration(0);
        timerTask->generation = m_cancelGeneration;
    }

    pushTask(timerTask);
    return timerTask->id;
}

TimerQueue::TimerId TimerQueue::runEvery(Duration interval, Task task)
{
    if (!started())
    {
        throw std::logic_error("TimerQueue is not started");
    }

    if (!task)
    {
        throw std::invalid_argument("TimerQueue task is empty");
    }

    if (interval.count() <= 0)
    {
        throw std::invalid_argument("TimerQueue interval must be greater than 0");
    }

    auto timerTask = std::make_shared<TimerTask>();
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        timerTask->id = ++m_nextId;
        timerTask->expiration = Clock::now() + interval;
        timerTask->interval = interval;
        timerTask->task = std::move(task);
        timerTask->repeat = true;
        timerTask->generation = m_cancelGeneration;
    }

    pushTask(timerTask);
    return timerTask->id;
}

bool TimerQueue::cancel(TimerId timerId)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto [_, inserted] = m_cancelled.emplace(timerId);
    if (inserted)
    {
        m_cv.notify_all();
    }
    return inserted;
}

void TimerQueue::cancelAll()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        ++m_cancelGeneration;
        m_cancelled.clear();
    }

    m_cv.notify_all();
}

std::size_t TimerQueue::size() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_tasks.size();
}

bool TimerQueue::isCancelledLocked(const std::shared_ptr<TimerTask>& timerTask) const noexcept
{
    return timerTask->generation < m_cancelGeneration || m_cancelled.contains(timerTask->id);
}

void TimerQueue::workerLoop(std::stop_token stopToken)
{
    while (!stopToken.stop_requested())
    {
        std::shared_ptr<TimerTask> timerTask;

        {
            std::unique_lock<std::mutex> lock(m_mutex);

            m_cv.wait(lock, [this, &stopToken]()
                      { return stopToken.stop_requested() || !m_tasks.empty() || m_stopped.load(std::memory_order_acquire); });

            if (stopToken.stop_requested() || m_stopped.load(std::memory_order_acquire))
            {
                break;
            }

            while (!m_tasks.empty())
            {
                auto top = m_tasks.top();

                if (isCancelledLocked(top))
                {
                    m_cancelled.erase(top->id);
                    m_tasks.pop();
                    continue;
                }

                const auto now = Clock::now();
                if (top->expiration > now)
                {
                    m_cv.wait_until(lock, top->expiration, [this, &stopToken, &top]()
                                    { return stopToken.stop_requested() || m_stopped.load(std::memory_order_acquire) || m_tasks.empty() || m_tasks.top()->id != top->id; });

                    if (stopToken.stop_requested() || m_stopped.load(std::memory_order_acquire))
                    {
                        return;
                    }

                    continue;
                }

                timerTask = top;
                m_tasks.pop();
                break;
            }
        }

        if (!timerTask)
        {
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (isCancelledLocked(timerTask))
            {
                m_cancelled.erase(timerTask->id);
                continue;
            }
        }

        executeTask(timerTask);

        if (timerTask->repeat)
        {
            bool cancelled = false;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (isCancelledLocked(timerTask))
                {
                    m_cancelled.erase(timerTask->id);
                    cancelled = true;
                }
            }

            if (!cancelled && !m_stopped.load(std::memory_order_acquire) && !stopToken.stop_requested())
            {
                timerTask->expiration = Clock::now() + timerTask->interval;
                pushTask(std::move(timerTask));
            }
        }
    }
}

void TimerQueue::executeTask(const std::shared_ptr<TimerTask>& timerTask)
{
    if (m_threadPool != nullptr)
    {
        m_threadPool->submit([task = timerTask->task]()
                             { task(); });
        return;
    }

    timerTask->task();
}

void TimerQueue::pushTask(std::shared_ptr<TimerTask> timerTask)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_tasks.emplace(std::move(timerTask));
    }
    m_cv.notify_all();
}

}  // namespace dbase::thread