#pragma once

#include <cstdint>
#include <random>
#include <span>
#include <vector>
#include <algorithm>
#include <stdexcept>

namespace dbase::random
{
class Random
{
    public:
        using Engine = std::mt19937_64;

        Random();
        explicit Random(std::uint64_t seed);

        void seed(std::uint64_t value) noexcept;

        [[nodiscard]] std::uint64_t nextU64() noexcept;
        [[nodiscard]] std::uint32_t nextU32() noexcept;

        [[nodiscard]] std::int32_t uniformInt(std::int32_t minValue, std::int32_t maxValue);
        [[nodiscard]] std::int64_t uniformInt64(std::int64_t minValue, std::int64_t maxValue);
        [[nodiscard]] double uniformReal(double minValue = 0.0, double maxValue = 1.0);
        [[nodiscard]] bool bernoulli(double probability = 0.5);

        template <typename Iter>
        void shuffle(Iter first, Iter last)
        {
            std::shuffle(first, last, m_engine);
        }

        template <typename T>
        [[nodiscard]] const T& choice(std::span<const T> items)
        {
            if (items.empty())
            {
                throw std::invalid_argument("Random::choice items is empty");
            }

            const auto index = static_cast<std::size_t>(uniformInt64(0, static_cast<std::int64_t>(items.size() - 1)));
            return items[index];
        }

        template <typename Weight>
        [[nodiscard]] std::size_t weightedIndex(std::span<const Weight> weights)
        {
            if (weights.empty())
            {
                throw std::invalid_argument("Random::weightedIndex weights is empty");
            }

            std::vector<double> normalized;
            normalized.reserve(weights.size());

            double total = 0.0;
            for (const auto& w : weights)
            {
                const double v = static_cast<double>(w);
                if (v < 0.0)
                {
                    throw std::invalid_argument("Random::weightedIndex negative weight");
                }

                normalized.emplace_back(v);
                total += v;
            }

            if (total <= 0.0)
            {
                throw std::invalid_argument("Random::weightedIndex total weight must be positive");
            }

            std::discrete_distribution<std::size_t> dist(normalized.begin(), normalized.end());
            return dist(m_engine);
        }

        [[nodiscard]] Engine& engine() noexcept
        {
            return m_engine;
        }

    private:
        Engine m_engine;
};

[[nodiscard]] Random& threadLocal() noexcept;

[[nodiscard]] std::uint64_t nextU64() noexcept;
[[nodiscard]] std::uint32_t nextU32() noexcept;
[[nodiscard]] std::int32_t uniformInt(std::int32_t minValue, std::int32_t maxValue);
[[nodiscard]] std::int64_t uniformInt64(std::int64_t minValue, std::int64_t maxValue);
[[nodiscard]] double uniformReal(double minValue = 0.0, double maxValue = 1.0);
[[nodiscard]] bool bernoulli(double probability = 0.5);

template <typename Iter>
inline void shuffle(Iter first, Iter last)
{
    threadLocal().shuffle(first, last);
}

template <typename T>
[[nodiscard]] inline const T& choice(std::span<const T> items)
{
    return threadLocal().choice(items);
}

template <typename Weight>
[[nodiscard]] inline std::size_t weightedIndex(std::span<const Weight> weights)
{
    return threadLocal().weightedIndex(weights);
}

}  // namespace dbase::random