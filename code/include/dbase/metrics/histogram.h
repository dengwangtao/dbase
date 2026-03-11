#pragma once

#include "dbase/metrics/metric.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <numeric>
#include <string>
#include <vector>

namespace dbase::metrics
{
struct HistogramSnapshot
{
        std::uint64_t count{0};
        double sum{0.0};
        double min{0.0};
        double max{0.0};
        double avg{0.0};
        double p50{0.0};
        double p90{0.0};
        double p95{0.0};
        double p99{0.0};
};

class Histogram final : public Metric
{
    public:
        explicit Histogram(std::string name, std::size_t maxSamples = 4096)
            : Metric(std::move(name)),
              m_maxSamples(maxSamples == 0 ? 1 : maxSamples)
        {
            m_samples.reserve(m_maxSamples);
        }

        [[nodiscard]] MetricType type() const noexcept override
        {
            return MetricType::Histogram;
        }

        void observe(double value)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            ++m_totalCount;
            m_totalSum += value;

            if (m_samples.size() < m_maxSamples)
            {
                m_samples.emplace_back(value);
                return;
            }

            m_samples[m_nextIndex] = value;
            ++m_nextIndex;
            if (m_nextIndex >= m_maxSamples)
            {
                m_nextIndex = 0;
            }
        }

        [[nodiscard]] HistogramSnapshot snapshot() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            HistogramSnapshot result;
            result.count = m_totalCount;
            result.sum = m_totalSum;

            if (m_samples.empty())
            {
                return result;
            }

            std::vector<double> sorted = m_samples;
            std::sort(sorted.begin(), sorted.end());

            result.min = sorted.front();
            result.max = sorted.back();
            result.avg = result.count == 0 ? 0.0 : result.sum / static_cast<double>(result.count);
            result.p50 = sorted[indexForPercentile(sorted.size(), 0.50)];
            result.p90 = sorted[indexForPercentile(sorted.size(), 0.90)];
            result.p95 = sorted[indexForPercentile(sorted.size(), 0.95)];
            result.p99 = sorted[indexForPercentile(sorted.size(), 0.99)];

            return result;
        }

        [[nodiscard]] std::size_t sampleCount() const
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_samples.size();
        }

    private:
        [[nodiscard]] static std::size_t indexForPercentile(std::size_t size, double percentile) noexcept
        {
            if (size == 0)
            {
                return 0;
            }

            const auto raw = percentile * static_cast<double>(size - 1);
            const auto idx = static_cast<std::size_t>(raw);
            return idx >= size ? size - 1 : idx;
        }

    private:
        const std::size_t m_maxSamples;
        mutable std::mutex m_mutex;
        std::vector<double> m_samples;
        std::size_t m_nextIndex{0};
        std::uint64_t m_totalCount{0};
        double m_totalSum{0.0};
};
}  // namespace dbase::metrics
