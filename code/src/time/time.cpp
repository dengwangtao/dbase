#include "dbase/time/time.h"

#include <ctime>
#include <thread>

namespace dbase::time
{
namespace
{
std::tm localTime(std::time_t value)
{
    std::tm tmValue{};
#if defined(_WIN32)
    localtime_s(&tmValue, &value);
#else
    localtime_r(&value, &tmValue);
#endif
    return tmValue;
}

std::string formatTimePoint(const SystemClock::time_point& tp, std::string_view format)
{
    const auto tt = SystemClock::to_time_t(tp);
    const auto tmValue = localTime(tt);

    std::string out(128, '\0');
    const auto size = std::strftime(out.data(), out.size(), std::string(format).c_str(), &tmValue);
    out.resize(size);
    return out;
}
}  // namespace

Timestamp Timestamp::now() noexcept
{
    return Timestamp(dbase::time::nowUs());
}

std::string Timestamp::format(std::string_view format) const
{
    return formatTimestampUs(m_unixUs, format);
}

std::int64_t nowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   SystemClock::now().time_since_epoch())
            .count();
}

std::int64_t nowUs()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
                   SystemClock::now().time_since_epoch())
            .count();
}

std::int64_t nowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
                   SystemClock::now().time_since_epoch())
            .count();
}

std::int64_t steadyNowNs()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   SteadyClock::now().time_since_epoch())
            .count();
}

std::int64_t steadyNowUs()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
                   SteadyClock::now().time_since_epoch())
            .count();
}

std::int64_t steadyNowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
                   SteadyClock::now().time_since_epoch())
            .count();
}

std::string formatNow(std::string_view format)
{
    return formatTimePoint(SystemClock::now(), format);
}

std::string formatTimestampMs(std::int64_t timestampMs, std::string_view format)
{
    const auto tp = SystemClock::time_point(std::chrono::milliseconds(timestampMs));
    return formatTimePoint(tp, format);
}

std::string formatTimestampUs(std::int64_t timestampUs, std::string_view format)
{
    const auto tp = SystemClock::time_point(std::chrono::microseconds(timestampUs));
    return formatTimePoint(tp, format);
}

Stopwatch::Stopwatch() noexcept
    : m_begin(SteadyClock::now())
{
}

void Stopwatch::reset() noexcept
{
    m_begin = SteadyClock::now();
}

std::int64_t Stopwatch::elapsedNs() const noexcept
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   SteadyClock::now() - m_begin)
            .count();
}

std::int64_t Stopwatch::elapsedUs() const noexcept
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
                   SteadyClock::now() - m_begin)
            .count();
}

std::int64_t Stopwatch::elapsedMs() const noexcept
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
                   SteadyClock::now() - m_begin)
            .count();
}

double Stopwatch::elapsedSeconds() const noexcept
{
    return std::chrono::duration<double>(SteadyClock::now() - m_begin).count();
}

}  // namespace dbase::time