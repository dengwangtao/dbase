#pragma once

#include "dbase/thread/serial_executor.h"

#include <cstddef>
#include <future>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace dbase::thread
{
class ShardedExecutor
{
    public:
        explicit ShardedExecutor(
                ThreadPool& threadPool,
                std::size_t shardCount,
                std::size_t queueCapacityPerShard = 0);

        ShardedExecutor(const ShardedExecutor&) = delete;
        ShardedExecutor& operator=(const ShardedExecutor&) = delete;

        ShardedExecutor(ShardedExecutor&&) = delete;
        ShardedExecutor& operator=(ShardedExecutor&&) = delete;

        ~ShardedExecutor();

        void start();
        void stop();

        [[nodiscard]] bool started() const noexcept;
        [[nodiscard]] bool stopped() const noexcept;

        [[nodiscard]] std::size_t shardCount() const noexcept;
        [[nodiscard]] std::size_t queueCapacityPerShard() const noexcept;
        [[nodiscard]] std::size_t pendingTaskCount() const noexcept;
        [[nodiscard]] std::size_t pendingTaskCount(std::size_t shardIndex) const;

        template <typename Key, typename F, typename... Args>
        auto submit(const Key& key, F&& f, Args&&... args)
                -> std::future<std::invoke_result_t<std::decay_t<F>, std::decay_t<Args>...>>
        {
            return executorForKey(key).submit(std::forward<F>(f), std::forward<Args>(args)...);
        }

        template <typename Key>
        [[nodiscard]] std::size_t shardIndexFor(const Key& key) const
        {
            return std::hash<Key>{}(key) % m_shardCount;
        }

        template <typename Key>
        [[nodiscard]] SerialExecutor& executorForKey(const Key& key)
        {
            return *m_executors[shardIndexFor(key)];
        }

        template <typename Key>
        [[nodiscard]] const SerialExecutor& executorForKey(const Key& key) const
        {
            return *m_executors[shardIndexFor(key)];
        }

        [[nodiscard]] SerialExecutor& executorAt(std::size_t shardIndex);
        [[nodiscard]] const SerialExecutor& executorAt(std::size_t shardIndex) const;

    private:
        ThreadPool& m_threadPool;
        const std::size_t m_shardCount{0};
        const std::size_t m_queueCapacityPerShard{0};
        std::vector<std::unique_ptr<SerialExecutor>> m_executors;
};
}  // namespace dbase::thread