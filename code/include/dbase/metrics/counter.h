#pragma once

#include "dbase/metrics/metric.h"

#include <atomic>
#include <cstdint>

namespace dbase::metrics
{
class Counter final : public Metric
{
    public:
        explicit Counter(std::string name)
            : Metric(std::move(name))
        {
        }

        [[nodiscard]] MetricType type() const noexcept override
        {
            return MetricType::Counter;
        }

        void inc(std::int64_t value = 1) noexcept
        {
            m_value.fetch_add(value, std::memory_order_relaxed);
        }

        [[nodiscard]] std::int64_t value() const noexcept
        {
            return m_value.load(std::memory_order_relaxed);
        }

        void reset(std::int64_t value = 0) noexcept
        {
            m_value.store(value, std::memory_order_relaxed);
        }

    private:
        std::atomic<std::int64_t> m_value{0};
};
}  // namespace dbase::metrics