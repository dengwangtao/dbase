#pragma once
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>

namespace dbase::sync
{
template <typename T>
class BlockingQueue
{
    public:
        explicit BlockingQueue(std::size_t capacity = 0)
            : m_capacity(capacity)
        {
        }

        BlockingQueue(const BlockingQueue&) = delete;
        BlockingQueue& operator=(const BlockingQueue&) = delete;

        void push(const T& value)
        {
            emplace(value);
        }

        void push(T&& value)
        {
            emplace(std::move(value));
        }

        template <typename... Args>
        void emplace(Args&&... args)
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            waitUntilPushable(lock);

            if (m_stopped)
            {
                throw std::runtime_error("BlockingQueue stopped");
            }

            m_queue.emplace_back(std::forward<Args>(args)...);
            lock.unlock();
            m_notEmpty.notify_one();
        }

        [[nodiscard]] T pop()
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_notEmpty.wait(lock, [this]()
                            { return m_stopped || !m_queue.empty(); });

            if (m_queue.empty())
            {
                throw std::runtime_error("BlockingQueue stopped");
            }

            T value = std::move(m_queue.front());
            m_queue.pop_front();
            lock.unlock();
            m_notFull.notify_one();
            return value;
        }

        [[nodiscard]] std::optional<T> popFor(std::uint64_t timeoutMs)
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            const bool ready = m_notEmpty.wait_for(
                    lock,
                    std::chrono::milliseconds(timeoutMs),
                    [this]()
                    {
                        return m_stopped || !m_queue.empty();
                    });

            if (!ready || m_queue.empty())
            {
                return std::nullopt;
            }

            T value = std::move(m_queue.front());
            m_queue.pop_front();
            lock.unlock();
            m_notFull.notify_one();
            return value;
        }

        [[nodiscard]] bool tryPush(const T& value)
        {
            return tryEmplaceImpl(value);
        }

        [[nodiscard]] bool tryPush(T&& value)
        {
            return tryEmplaceImpl(std::move(value));
        }

        [[nodiscard]] std::optional<T> tryPop()
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (m_queue.empty())
            {
                return std::nullopt;
            }

            T value = std::move(m_queue.front());
            m_queue.pop_front();
            lock.unlock();
            m_notFull.notify_one();
            return value;
        }

        void stop()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_stopped)
            {
                return;
            }
            m_stopped = true;
            m_notEmpty.notify_all();
            m_notFull.notify_all();
        }

        [[nodiscard]] bool stopped() const noexcept
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_stopped;
        }

        [[nodiscard]] bool empty() const noexcept
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_queue.empty();
        }

        [[nodiscard]] std::size_t size() const noexcept
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_queue.size();
        }

        [[nodiscard]] std::size_t capacity() const noexcept
        {
            return m_capacity;
        }

    private:
        void waitUntilPushable(std::unique_lock<std::mutex>& lock)
        {
            m_notFull.wait(lock, [this]()
                           { return m_stopped || m_capacity == 0 || m_queue.size() < m_capacity; });
        }

        template <typename U>
        [[nodiscard]] bool tryEmplaceImpl(U&& value)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_stopped)
            {
                return false;
            }

            if (m_capacity != 0 && m_queue.size() >= m_capacity)
            {
                return false;
            }

            m_queue.emplace_back(std::forward<U>(value));
            m_notEmpty.notify_one();
            return true;
        }

    private:
        const std::size_t m_capacity{0};
        mutable std::mutex m_mutex;
        std::condition_variable m_notEmpty;
        std::condition_variable m_notFull;
        std::deque<T> m_queue;
        bool m_stopped{false};
};
}  // namespace dbase::sync