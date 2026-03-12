#pragma once

#include "dbase/sync/blocking_queue.h"
#include "dbase/thread/thread.h"

#include <atomic>
#include <cstddef>
#include <future>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace dbase::thread
{
class ThreadPool
{
    public:
        using Task = std::function<void()>;

        ThreadPool(
                std::size_t threadCount,
                std::string threadNamePrefix = "worker",
                std::size_t queueCapacity = 0);

        ThreadPool(const ThreadPool&) = delete;
        ThreadPool& operator=(const ThreadPool&) = delete;

        ThreadPool(ThreadPool&&) = delete;
        ThreadPool& operator=(ThreadPool&&) = delete;

        ~ThreadPool();

        void start();
        void stop();

        [[nodiscard]] bool started() const noexcept;
        [[nodiscard]] bool stopped() const noexcept;

        [[nodiscard]] std::size_t threadCount() const noexcept;
        [[nodiscard]] std::size_t queueCapacity() const noexcept;
        [[nodiscard]] std::size_t pendingTaskCount() const noexcept;

        template <typename F, typename... Args>
        auto submit(F&& f, Args&&... args)
                -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
        {
            using ReturnType = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;

            if (!started())
            {
                throw std::logic_error("ThreadPool is not started");
            }

            auto taskPtr = std::make_shared<std::packaged_task<ReturnType()>>(
                    [func = std::forward<F>(f), ... capturedArgs = std::forward<Args>(args)]() mutable -> ReturnType
                    {
                        return std::invoke(std::move(func), std::move(capturedArgs)...);
                    });

            auto future = taskPtr->get_future();

            try
            {
                m_tasks.push([taskPtr]()
                             { (*taskPtr)(); });
            }
            catch (const std::runtime_error&)
            {
                throw std::logic_error("ThreadPool is stopped");
            }

            return future;
        }

    private:
        void workerLoop(std::size_t index);

    private:
        const std::size_t m_threadCount{0};
        const std::size_t m_queueCapacity{0};
        const std::string m_threadNamePrefix;

        dbase::sync::BlockingQueue<Task> m_tasks;
        std::vector<dbase::thread::Thread> m_workers;

        std::atomic<bool> m_started{false};
        std::atomic<bool> m_stopped{true};
};
}  // namespace dbase::thread