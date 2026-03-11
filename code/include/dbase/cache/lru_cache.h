#pragma once

#include "dbase/error/error.h"

#include <cstddef>
#include <functional>
#include <list>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace dbase::cache
{
template <typename K, typename V, typename Hash = std::hash<K>, typename KeyEqual = std::equal_to<K>>
class LRUCache
{
    public:
        using key_type = K;
        using mapped_type = V;
        using value_type = std::pair<K, V>;
        using EvictCallback = std::function<void(const K&, const V&)>;

    private:
        using ListType = std::list<value_type>;
        using ListIterator = typename ListType::iterator;
        using MapType = std::unordered_map<K, ListIterator, Hash, KeyEqual>;

    public:
        explicit LRUCache(std::size_t capacity)
            : m_capacity(capacity)
        {
            if (m_capacity == 0)
            {
                throw std::invalid_argument("LRUCache capacity must be greater than 0");
            }
        }

        LRUCache(const LRUCache&) = delete;
        LRUCache& operator=(const LRUCache&) = delete;

        LRUCache(LRUCache&&) = delete;
        LRUCache& operator=(LRUCache&&) = delete;

        void setEvictCallback(EvictCallback cb)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_evictCallback = std::move(cb);
        }

        [[nodiscard]] std::size_t capacity() const noexcept
        {
            return m_capacity;
        }

        [[nodiscard]] std::size_t size() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_index.size();
        }

        [[nodiscard]] bool empty() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_index.empty();
        }

        [[nodiscard]] bool contains(const K& key) const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_index.find(key) != m_index.end();
        }

        void clear()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_items.clear();
            m_index.clear();
        }

        void put(const K& key, const V& value)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            putImpl(key, value);
        }

        void put(const K& key, V&& value)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            putImpl(key, std::move(value));
        }

        void put(K&& key, const V& value)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            putImpl(std::move(key), value);
        }

        void put(K&& key, V&& value)
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            putImpl(std::move(key), std::move(value));
        }

        [[nodiscard]] std::optional<V> get(const K& key)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            const auto it = m_index.find(key);
            if (it == m_index.end())
            {
                return std::nullopt;
            }

            touch(it->second);
            return it->second->second;
        }

        [[nodiscard]] dbase::Result<V> getOrError(const K& key)
        {
            auto value = get(key);
            if (!value.has_value())
            {
                return dbase::makeErrorResult<V>(dbase::ErrorCode::NotFound, "cache key not found");
            }

            return std::move(*value);
        }

        [[nodiscard]] std::optional<V> peek(const K& key) const
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            const auto it = m_index.find(key);
            if (it == m_index.end())
            {
                return std::nullopt;
            }

            return it->second->second;
        }

        [[nodiscard]] bool remove(const K& key)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            const auto it = m_index.find(key);
            if (it == m_index.end())
            {
                return false;
            }

            m_items.erase(it->second);
            m_index.erase(it);
            return true;
        }

        [[nodiscard]] std::optional<value_type> popLru()
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_items.empty())
            {
                return std::nullopt;
            }

            auto& node = m_items.back();
            value_type result = node;
            m_index.erase(node.first);
            m_items.pop_back();
            return result;
        }

        [[nodiscard]] std::optional<value_type> popMru()
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_items.empty())
            {
                return std::nullopt;
            }

            auto& node = m_items.front();
            value_type result = node;
            m_index.erase(node.first);
            m_items.pop_front();
            return result;
        }

        [[nodiscard]] std::optional<value_type> lru() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_items.empty())
            {
                return std::nullopt;
            }

            return m_items.back();
        }

        [[nodiscard]] std::optional<value_type> mru() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (m_items.empty())
            {
                return std::nullopt;
            }

            return m_items.front();
        }

    private:
        template <typename KeyArg, typename ValueArg>
        void putImpl(KeyArg&& key, ValueArg&& value)
        {
            auto it = m_index.find(key);
            if (it != m_index.end())
            {
                it->second->second = std::forward<ValueArg>(value);
                touch(it->second);
                return;
            }

            m_items.emplace_front(std::forward<KeyArg>(key), std::forward<ValueArg>(value));
            m_index.emplace(m_items.front().first, m_items.begin());

            evictIfNeeded();
        }

        void touch(ListIterator it)
        {
            if (it != m_items.begin())
            {
                m_items.splice(m_items.begin(), m_items, it);
            }
        }

        void evictIfNeeded()
        {
            while (m_index.size() > m_capacity)
            {
                auto& node = m_items.back();

                if (m_evictCallback)
                {
                    m_evictCallback(node.first, node.second);
                }

                m_index.erase(node.first);
                m_items.pop_back();
            }
        }

    private:
        const std::size_t m_capacity;
        mutable std::mutex m_mutex;
        ListType m_items;
        MapType m_index;
        EvictCallback m_evictCallback;
};
}  // namespace dbase::cache