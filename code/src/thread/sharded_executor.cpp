#include "dbase/thread/sharded_executor.h"

#include <stdexcept>

namespace dbase::thread
{
ShardedExecutor::ShardedExecutor(
        ThreadPool& threadPool,
        std::size_t shardCount,
        std::size_t queueCapacityPerShard)
    : m_threadPool(threadPool),
      m_shardCount(shardCount),
      m_queueCapacityPerShard(queueCapacityPerShard)
{
    if (m_shardCount == 0)
    {
        throw std::invalid_argument("ShardedExecutor shardCount must be greater than 0");
    }

    m_executors.reserve(m_shardCount);
    for (std::size_t i = 0; i < m_shardCount; ++i)
    {
        m_executors.emplace_back(std::make_unique<SerialExecutor>(m_threadPool, m_queueCapacityPerShard));
    }
}

ShardedExecutor::~ShardedExecutor()
{
    stop();
}

void ShardedExecutor::start()
{
    try
    {
        for (auto& executor : m_executors)
        {
            executor->start();
        }
    }
    catch (...)
    {
        stop();
        throw;
    }
}

void ShardedExecutor::stop()
{
    for (auto& executor : m_executors)
    {
        executor->stop();
    }
}

bool ShardedExecutor::started() const noexcept
{
    for (const auto& executor : m_executors)
    {
        if (!executor->started())
        {
            return false;
        }
    }
    return true;
}

bool ShardedExecutor::stopped() const noexcept
{
    for (const auto& executor : m_executors)
    {
        if (!executor->stopped())
        {
            return false;
        }
    }
    return true;
}

std::size_t ShardedExecutor::shardCount() const noexcept
{
    return m_shardCount;
}

std::size_t ShardedExecutor::queueCapacityPerShard() const noexcept
{
    return m_queueCapacityPerShard;
}

std::size_t ShardedExecutor::pendingTaskCount() const noexcept
{
    std::size_t total = 0;
    for (const auto& executor : m_executors)
    {
        total += executor->pendingTaskCount();
    }
    return total;
}

std::size_t ShardedExecutor::pendingTaskCount(std::size_t shardIndex) const
{
    return executorAt(shardIndex).pendingTaskCount();
}

SerialExecutor& ShardedExecutor::executorAt(std::size_t shardIndex)
{
    if (shardIndex >= m_shardCount)
    {
        throw std::out_of_range("ShardedExecutor shard index out of range");
    }

    return *m_executors[shardIndex];
}

const SerialExecutor& ShardedExecutor::executorAt(std::size_t shardIndex) const
{
    if (shardIndex >= m_shardCount)
    {
        throw std::out_of_range("ShardedExecutor shard index out of range");
    }

    return *m_executors[shardIndex];
}

}  // namespace dbase::thread