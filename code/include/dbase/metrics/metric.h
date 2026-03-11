#pragma once

#include <string>

namespace dbase::metrics
{
enum class MetricType
{
    Counter,
    Gauge,
    Histogram
};

class Metric
{
    public:
        explicit Metric(std::string name)
            : m_name(std::move(name))
        {
        }

        virtual ~Metric() = default;

        Metric(const Metric&) = delete;
        Metric& operator=(const Metric&) = delete;

        [[nodiscard]] const std::string& name() const noexcept
        {
            return m_name;
        }

        [[nodiscard]] virtual MetricType type() const noexcept = 0;

    private:
        std::string m_name;
};
}  // namespace dbase::metrics