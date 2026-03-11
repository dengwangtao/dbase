#include "dbase/metrics/registry.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace dbase::metrics
{
Counter& MetricRegistry::counter(std::string name)
{
    return getOrCreate<Counter>(std::move(name), MetricType::Counter);
}

Gauge& MetricRegistry::gauge(std::string name)
{
    return getOrCreate<Gauge>(std::move(name), MetricType::Gauge);
}

Histogram& MetricRegistry::histogram(std::string name, std::size_t maxSamples)
{
    return getOrCreate<Histogram>(std::move(name), MetricType::Histogram, maxSamples);
}

dbase::Result<Metric*> MetricRegistry::find(std::string_view name) const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    const auto it = m_metrics.find(std::string(name));
    if (it == m_metrics.end())
    {
        return dbase::makeErrorResult<Metric*>(dbase::ErrorCode::NotFound, "metric not found: " + std::string(name));
    }

    return it->second.get();
}

std::string MetricRegistry::dumpText() const
{
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6);

    for (const auto& [name, metric] : m_metrics)
    {
        switch (metric->type())
        {
            case MetricType::Counter:
            {
                const auto& counter = static_cast<const Counter&>(*metric);
                oss << name << ' ' << counter.value() << '\n';
                break;
            }

            case MetricType::Gauge:
            {
                const auto& gauge = static_cast<const Gauge&>(*metric);
                oss << name << ' ' << gauge.value() << '\n';
                break;
            }

            case MetricType::Histogram:
            {
                const auto& histogram = static_cast<const Histogram&>(*metric);
                const auto snap = histogram.snapshot();

                oss << name << ".count " << snap.count << '\n';
                oss << name << ".sum " << snap.sum << '\n';
                oss << name << ".min " << snap.min << '\n';
                oss << name << ".max " << snap.max << '\n';
                oss << name << ".avg " << snap.avg << '\n';
                oss << name << ".p50 " << snap.p50 << '\n';
                oss << name << ".p90 " << snap.p90 << '\n';
                oss << name << ".p95 " << snap.p95 << '\n';
                oss << name << ".p99 " << snap.p99 << '\n';
                break;
            }
        }
    }

    return oss.str();
}

}  // namespace dbase::metrics