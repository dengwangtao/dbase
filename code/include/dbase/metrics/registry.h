#pragma once

#include "dbase/error/error.h"
#include "dbase/metrics/counter.h"
#include "dbase/metrics/gauge.h"
#include "dbase/metrics/histogram.h"

#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace dbase::metrics
{
class MetricRegistry
{
    public:
        MetricRegistry() = default;

        MetricRegistry(const MetricRegistry&) = delete;
        MetricRegistry& operator=(const MetricRegistry&) = delete;

        Counter& counter(std::string name);
        Gauge& gauge(std::string name);
        Histogram& histogram(std::string name, std::size_t maxSamples = 4096);

        [[nodiscard]] dbase::Result<Metric*> find(std::string_view name) const;

        [[nodiscard]] std::string dumpText() const;

    private:
        template <typename T, typename... Args>
        T& getOrCreate(std::string name, MetricType expectedType, Args&&... args)
        {
            std::lock_guard<std::mutex> lock(m_mutex);

            const auto it = m_metrics.find(name);
            if (it != m_metrics.end())
            {
                if (it->second->type() != expectedType)
                {
                    throw std::logic_error("MetricRegistry type mismatch for metric: " + name);
                }

                return static_cast<T&>(*it->second);
            }

            auto metric = std::make_unique<T>(std::move(name), std::forward<Args>(args)...);
            auto* ptr = metric.get();
            m_metrics.emplace(ptr->name(), std::move(metric));
            return *ptr;
        }

    private:
        mutable std::mutex m_mutex;
        std::unordered_map<std::string, std::unique_ptr<Metric>> m_metrics;
};
}  // namespace dbase::metrics