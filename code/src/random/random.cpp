#include "dbase/random/random.h"

#include <chrono>
#include <stdexcept>
#include <thread>

namespace dbase::random
{
namespace
{
[[nodiscard]] std::uint64_t makeSeed() noexcept
{
    const auto now = static_cast<std::uint64_t>(
            std::chrono::high_resolution_clock::now().time_since_epoch().count());

    const auto tid = static_cast<std::uint64_t>(
            std::hash<std::thread::id>{}(std::this_thread::get_id()));

    return now ^ (tid + 0x9e3779b97f4a7c15ULL + (now << 6) + (now >> 2));
}
}  // namespace

Random::Random()
    : m_engine(makeSeed())
{
}

Random::Random(std::uint64_t seed)
    : m_engine(seed)
{
}

void Random::seed(std::uint64_t value) noexcept
{
    m_engine.seed(value);
}

std::uint64_t Random::nextU64() noexcept
{
    return m_engine();
}

std::uint32_t Random::nextU32() noexcept
{
    return static_cast<std::uint32_t>(m_engine());
}

std::int32_t Random::uniformInt(std::int32_t minValue, std::int32_t maxValue)
{
    if (minValue > maxValue)
    {
        throw std::invalid_argument("Random::uniformInt minValue > maxValue");
    }

    std::uniform_int_distribution<std::int32_t> dist(minValue, maxValue);
    return dist(m_engine);
}

std::int64_t Random::uniformInt64(std::int64_t minValue, std::int64_t maxValue)
{
    if (minValue > maxValue)
    {
        throw std::invalid_argument("Random::uniformInt64 minValue > maxValue");
    }

    std::uniform_int_distribution<std::int64_t> dist(minValue, maxValue);
    return dist(m_engine);
}

double Random::uniformReal(double minValue, double maxValue)
{
    if (minValue > maxValue)
    {
        throw std::invalid_argument("Random::uniformReal minValue > maxValue");
    }

    std::uniform_real_distribution<double> dist(minValue, maxValue);
    return dist(m_engine);
}

bool Random::bernoulli(double probability)
{
    if (probability < 0.0 || probability > 1.0)
    {
        throw std::invalid_argument("Random::bernoulli probability out of range");
    }

    std::bernoulli_distribution dist(probability);
    return dist(m_engine);
}

Random& threadLocal() noexcept
{
    thread_local Random rng;
    return rng;
}

std::uint64_t nextU64() noexcept
{
    return threadLocal().nextU64();
}

std::uint32_t nextU32() noexcept
{
    return threadLocal().nextU32();
}

std::int32_t uniformInt(std::int32_t minValue, std::int32_t maxValue)
{
    return threadLocal().uniformInt(minValue, maxValue);
}

std::int64_t uniformInt64(std::int64_t minValue, std::int64_t maxValue)
{
    return threadLocal().uniformInt64(minValue, maxValue);
}

double uniformReal(double minValue, double maxValue)
{
    return threadLocal().uniformReal(minValue, maxValue);
}

bool bernoulli(double probability)
{
    return threadLocal().bernoulli(probability);
}

}  // namespace dbase::random