#pragma once

#include <atomic>
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
            : m_maxIdle(maxIdle), m_lifetime(std::make_shared<LifetimeToken>())
        {
            m_idle.reserve(maxIdle);
        }

        ~ObjectPool() = default;

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
            if (count == 0 || m_maxIdle == 0)
            {
                return;
            }

            std::size_t target = 0;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (m_idle.size() >= count || m_idle.size() >= m_maxIdle)
                {
                    return;
                }
                target = (count < m_maxIdle ? count : m_maxIdle) - m_idle.size();
            }

            std::vector<std::unique_ptr<T>> created;
            created.reserve(target);
            for (std::size_t i = 0; i < target; ++i)
            {
                created.emplace_back(createObject());
            }

            std::lock_guard<std::mutex> lock(m_mutex);
            for (auto& obj : created)
            {
                if (m_idle.size() >= m_maxIdle)
                {
                    break;
                }
                m_idle.emplace_back(std::move(obj));
            }
        }

        [[nodiscard]] std::unique_ptr<T> acquire()
        {
            m_totalAcquired.fetch_add(1, std::memory_order_relaxed);

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (!m_idle.empty())
                {
                    auto ptr = std::move(m_idle.back());
                    m_idle.pop_back();
                    return ptr;
                }
            }

            return createObject();
        }

        void release(std::unique_ptr<T> obj)
        {
            if (!obj)
            {
                return;
            }

            ResetCallback reset;
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                reset = m_resetCallback;
            }

            if (reset)
            {
                reset(*obj);
            }

            m_totalReleased.fetch_add(1, std::memory_order_relaxed);

            if (m_maxIdle == 0)
            {
                return;
            }

            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_idle.size() < m_maxIdle)
            {
                m_idle.emplace_back(std::move(obj));
            }
        }

        template <typename... Args>
        [[nodiscard]] std::unique_ptr<T> create(Args&&... args)
        {
            m_totalCreated.fetch_add(1, std::memory_order_relaxed);
            return std::make_unique<T>(std::forward<Args>(args)...);
        }

        [[nodiscard]] std::shared_ptr<T> makeShared()
        {
            auto unique = acquire();
            T* raw = unique.release();
            std::weak_ptr<LifetimeToken> weak = m_lifetime;

            return std::shared_ptr<T>(
                    raw,
                    [this, weak](T* ptr)
                    {
                        if (!ptr)
                        {
                            return;
                        }

                        if (weak.lock())
                        {
                            this->release(std::unique_ptr<T>(ptr));
                            return;
                        }

                        delete ptr;
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
            Stats s;
            s.totalCreated = m_totalCreated.load(std::memory_order_relaxed);
            s.totalAcquired = m_totalAcquired.load(std::memory_order_relaxed);
            s.totalReleased = m_totalReleased.load(std::memory_order_relaxed);
            s.maxIdle = m_maxIdle;

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                s.idleCount = m_idle.size();
            }

            return s;
        }

    private:
        struct LifetimeToken
        {
        };

        [[nodiscard]] FactoryCallback factoryCallbackCopy() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_factoryCallback;
        }

        [[nodiscard]] std::unique_ptr<T> createObject()
        {
            auto factory = factoryCallbackCopy();

            std::unique_ptr<T> obj;
            if (factory)
            {
                obj = factory();
            }

            if (!obj)
            {
                obj = std::make_unique<T>();
            }

            m_totalCreated.fetch_add(1, std::memory_order_relaxed);
            return obj;
        }

    private:
        const std::size_t m_maxIdle;
        mutable std::mutex m_mutex;
        std::vector<std::unique_ptr<T>> m_idle;
        ResetCallback m_resetCallback;
        FactoryCallback m_factoryCallback;
        std::shared_ptr<LifetimeToken> m_lifetime;

        std::atomic<std::size_t> m_totalCreated{0};
        std::atomic<std::size_t> m_totalAcquired{0};
        std::atomic<std::size_t> m_totalReleased{0};
};
}  // namespace dbase::memory