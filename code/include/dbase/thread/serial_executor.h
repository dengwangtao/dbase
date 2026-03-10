#pragma once

#include "dbase/sync/blocking_queue.h"
#include "dbase/thread/thread_pool.h"

#include <atomic>
#include <cstddef>
#include <future>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace dbase::thread
{
class SerialExecutor
{
    public:
        using Task = std::function<void()>;

        explicit SerialExecutor(ThreadPool& threadPool, std::size_t queueCapacity = 0);

        SerialExecutor(const SerialExecutor&) = delete;
        SerialExecutor& operator=(const SerialExecutor&) = delete;

        SerialExecutor(SerialExecutor&&) = delete;
        SerialExecutor& operator=(SerialExecutor&&) = delete;

        ~SerialExecutor();

        void start();
        void stop();

        [[nodiscard]] bool started() const noexcept;
        [[nodiscard]] bool stopped() const noexcept;
        [[nodiscard]] std::size_t pendingTaskCount() const noexcept;
        [[nodiscard]] std::size_t queueCapacity() const noexcept;

        template <typename F, typename... Args>
        auto submit(F&& f, Args&&... args)
                -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
        {
            using ReturnType = std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>;

            if (!started())
            {
                throw std::logic_error("SerialExecutor is not started");
            }

            auto taskPtr = std::make_shared<std::packaged_task<ReturnType()>>(
                    [func = std::forward<F>(f), ... capturedArgs = std::forward<Args>(args)]() mutable -> ReturnType
                    {
                        return std::invoke(std::move(func), std::move(capturedArgs)...);
                    });

            auto future = taskPtr->get_future();

            m_tasks.push([taskPtr]()
                         { (*taskPtr)(); });

            schedule();
            return future;
        }

    private:
        void schedule();
        void drain();

    private:
        ThreadPool& m_threadPool;
        dbase::sync::BlockingQueue<Task> m_tasks;
        std::atomic<bool> m_started{false};
        std::atomic<bool> m_stopped{false};
        std::atomic<bool> m_running{false};
};
}  // namespace dbase::thread