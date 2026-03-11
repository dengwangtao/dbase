#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <type_traits>
#include <utility>
#include <vector>

namespace dbase::memory
{
template <typename T>
class ObjectPool
{
        static_assert(!std::is_reference_v<T>, "ObjectPool<T> does not support reference types");
        static_assert(!std::is_const_v<T>, "ObjectPool<T> does not support const types");

    public:
        struct Stats
        {
                std::size_t totalCreated{0};
                std::size_t totalAcquired{0};
                std::size_t totalReleased{0};
                std::size_t idleCount{0};
                std::size_t maxIdle{0};
        };

        using ResetCallback = std::function<void(T&)>;
        using FactoryCallback = std::function<std::unique_ptr<T>()>;

        explicit ObjectPool(std::size_t maxIdle = 1024)
            : m_maxIdle(maxIdle)
        {
        }

        ObjectPool(const ObjectPool&) = delete;
        ObjectPool& operator=(const ObjectPool&) = delete;

        ObjectPool(ObjectPool&&) = delete;
        ObjectPool& operator=(ObjectPool&&) = delete;

        void setResetCallback(ResetCallback cb)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_resetCallback = std::move(cb);
        }

        void setFactoryCallback(FactoryCallback cb)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_factoryCallback = std::move(cb);
        }

        [[nodiscard]] std::size_t maxIdle() const noexcept
        {
            return m_maxIdle;
        }

        void reserve(std::size_t count)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            while (m_idle.size() < count && m_idle.size() < m_maxIdle)
            {
                m_idle.emplace_back(createObjectUnlocked());
            }
        }

        [[nodiscard]] std::unique_ptr<T> acquire()
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            ++m_totalAcquired;

            if (!m_idle.empty())
            {
                auto ptr = std::move(m_idle.back());
                m_idle.pop_back();
                return ptr;
            }

            return createObjectUnlocked();
        }

        void release(std::unique_ptr<T> obj)
        {
            if (!obj)
            {
                return;
            }

            std::lock_guard<std::mutex> lock(m_mutex);

            ++m_totalReleased;

            if (m_resetCallback)
            {
                m_resetCallback(*obj);
            }

            if (m_idle.size() < m_maxIdle)
            {
                m_idle.emplace_back(std::move(obj));
            }
        }

        template <typename... Args>
        [[nodiscard]] std::unique_ptr<T> create(Args&&... args)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            ++m_totalCreated;
            ++m_totalAcquired;
            return std::make_unique<T>(std::forward<Args>(args)...);
        }

        [[nodiscard]] std::shared_ptr<T> makeShared()
        {
            auto unique = acquire();
            return std::shared_ptr<T>(
                    unique.release(),
                    [this](T* ptr)
                    {
                        this->release(std::unique_ptr<T>(ptr));
                    });
        }

        [[nodiscard]] std::size_t idleCount() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_idle.size();
        }

        [[nodiscard]] bool empty() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_idle.empty();
        }

        void clear()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_idle.clear();
        }

        [[nodiscard]] Stats stats() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            Stats s;
            s.totalCreated = m_totalCreated;
            s.totalAcquired = m_totalAcquired;
            s.totalReleased = m_totalReleased;
            s.idleCount = m_idle.size();
            s.maxIdle = m_maxIdle;
            return s;
        }

    private:
        [[nodiscard]] std::unique_ptr<T> createObjectUnlocked()
        {
            ++m_totalCreated;

            if (m_factoryCallback)
            {
                auto obj = m_factoryCallback();
                if (obj)
                {
                    return obj;
                }
            }

            return std::make_unique<T>();
        }

    private:
        const std::size_t m_maxIdle;
        mutable std::mutex m_mutex;
        std::vector<std::unique_ptr<T>> m_idle;
        ResetCallback m_resetCallback;
        FactoryCallback m_factoryCallback;

        std::size_t m_totalCreated{0};
        std::size_t m_totalAcquired{0};
        std::size_t m_totalReleased{0};
};
}  // namespace dbase::memory