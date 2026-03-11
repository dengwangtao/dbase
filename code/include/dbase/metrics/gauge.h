#pragma once

#include "dbase/metrics/metric.h"

#include <atomic>
#include <cstdint>

namespace dbase::metrics
{
class Gauge final : public Metric
{
    public:
        explicit Gauge(std::string name)
            : Metric(std::move(name))
        {
        }

        [[nodiscard]] MetricType type() const noexcept override
        {
            return MetricType::Gauge;
        }

        void set(double value) noexcept
        {
            m_value.store(value, std::memory_order_relaxed);
        }

        void inc(double value = 1.0) noexcept
        {
            auto current = m_value.load(std::memory_order_relaxed);
            while (!m_value.compare_exchange_weak(
                    current,
                    current + value,
                    std::memory_order_relaxed,
                    std::memory_order_relaxed))
            {
            }
        }

        void dec(double value = 1.0) noexcept
        {
            inc(-value);
        }

        [[nodiscard]] double value() const noexcept
        {
            return m_value.load(std::memory_order_relaxed);
        }

    private:
        std::atomic<double> m_value{0.0};
};
}  // namespace dbase::metrics