#pragma once

#include "dbase/thread/thread.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <unordered_set>

namespace dbase::thread
{
class ThreadPool;

class TimerQueue
{
    public:
        using Clock = std::chrono::steady_clock;
        using Duration = std::chrono::milliseconds;
        using TimePoint = Clock::time_point;
        using Task = std::function<void()>;
        using TimerId = std::uint64_t;

        explicit TimerQueue(ThreadPool* threadPool = nullptr, std::string threadName = "timer");
        ~TimerQueue();

        TimerQueue(const TimerQueue&) = delete;
        TimerQueue& operator=(const TimerQueue&) = delete;

        TimerQueue(TimerQueue&&) = delete;
        TimerQueue& operator=(TimerQueue&&) = delete;

        void start();
        void stop();

        [[nodiscard]] bool started() const noexcept;
        [[nodiscard]] bool stopped() const noexcept;

        void setThreadPool(ThreadPool* threadPool);

        [[nodiscard]] TimerId runAfter(Duration delay, Task task);
        [[nodiscard]] TimerId runAt(TimePoint timePoint, Task task);
        [[nodiscard]] TimerId runEvery(Duration interval, Task task);

        bool cancel(TimerId timerId);
        void cancelAll();

        [[nodiscard]] std::size_t size() const noexcept;

    private:
        struct TimerTask
        {
                TimerId id{0};
                TimePoint expiration;
                Duration interval{0};
                Task task;
                bool repeat{false};
        };

        struct TimerTaskCompare
        {
                bool operator()(const std::shared_ptr<TimerTask>& lhs, const std::shared_ptr<TimerTask>& rhs) const noexcept
                {
                    if (lhs->expiration != rhs->expiration)
                    {
                        return lhs->expiration > rhs->expiration;
                    }
                    return lhs->id > rhs->id;
                }
        };

    private:
        void workerLoop(std::stop_token stopToken);
        void executeTask(const std::shared_ptr<TimerTask>& timerTask);
        void pushTask(std::shared_ptr<TimerTask> timerTask);

    private:
        mutable std::mutex m_mutex;
        std::condition_variable m_cv;
        std::priority_queue<
                std::shared_ptr<TimerTask>,
                std::vector<std::shared_ptr<TimerTask>>,
                TimerTaskCompare>
                m_tasks;
        std::unordered_set<TimerId> m_cancelled;
        std::atomic<TimerId> m_nextId{0};
        std::atomic<bool> m_started{false};
        std::atomic<bool> m_stopped{true};

        ThreadPool* m_threadPool{nullptr};
        std::string m_threadName{"timer"};
        dbase::thread::Thread m_thread;
};
}  // namespace dbase::thread