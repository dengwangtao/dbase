#include "dbase/time/time.h"

#include <ctime>

namespace dbase::time
{
namespace
{
std::tm localTime(std::time_t value)
{
    std::tm tm_value{};
#if defined(_WIN32)
    localtime_s(&tm_value, &value);
#else
    localtime_r(&value, &tm_value);
#endif
    return tm_value;
}
}  // namespace

std::int64_t nowMs()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
                   SystemClock::now().time_since_epoch())
            .count();
}

std::int64_t nowUs()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
                   SystemClock::now().time_since_epoch())
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
    return formatTimestampMs(nowMs(), format);
}

std::string formatTimestampMs(std::int64_t timestampMs, std::string_view format)
{
    const auto tp = SystemClock::time_point(std::chrono::milliseconds(timestampMs));
    const auto tt = SystemClock::to_time_t(tp);
    const auto tm_value = localTime(tt);

    std::string out(128, '\0');
    const auto size = std::strftime(out.data(), out.size(), std::string(format).c_str(), &tm_value);
    out.resize(size);
    return out;
}

Stopwatch::Stopwatch()
    : m_begin(SteadyClock::now())
{
}

void Stopwatch::reset() noexcept
{
    m_begin = SteadyClock::now();
}

std::int64_t Stopwatch::elapsedMs() const noexcept
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
                   SteadyClock::now() - m_begin)
            .count();
}

std::int64_t Stopwatch::elapsedUs() const noexcept
{
    return std::chrono::duration_cast<std::chrono::microseconds>(
                   SteadyClock::now() - m_begin)
            .count();
}

}  // namespace dbase::time