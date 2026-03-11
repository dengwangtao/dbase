#pragma once

#include <thread>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

namespace dbase::time
{
using SteadyClock = std::chrono::steady_clock;
using SystemClock = std::chrono::system_clock;

class Timestamp
{
    public:
        Timestamp() = default;
        explicit Timestamp(std::int64_t unixUs) noexcept
            : m_unixUs(unixUs)
        {
        }

        [[nodiscard]] static Timestamp now() noexcept;

        [[nodiscard]] std::int64_t unixMs() const noexcept
        {
            return m_unixUs / 1000;
        }

        [[nodiscard]] std::int64_t unixUs() const noexcept
        {
            return m_unixUs;
        }

        [[nodiscard]] SystemClock::time_point toTimePoint() const noexcept
        {
            return SystemClock::time_point(std::chrono::microseconds(m_unixUs));
        }

        [[nodiscard]] std::string format(std::string_view format = "%Y-%m-%d %H:%M:%S") const;

        [[nodiscard]] bool valid() const noexcept
        {
            return m_unixUs > 0;
        }

        [[nodiscard]] auto operator<=>(const Timestamp&) const noexcept = default;

    private:
        std::int64_t m_unixUs{0};
};

[[nodiscard]] std::int64_t nowNs();
[[nodiscard]] std::int64_t nowUs();
[[nodiscard]] std::int64_t nowMs();

[[nodiscard]] std::int64_t steadyNowNs();
[[nodiscard]] std::int64_t steadyNowUs();
[[nodiscard]] std::int64_t steadyNowMs();

[[nodiscard]] std::string formatNow(std::string_view format = "%Y-%m-%d %H:%M:%S");
[[nodiscard]] std::string formatTimestampMs(std::int64_t timestampMs, std::string_view format = "%Y-%m-%d %H:%M:%S");
[[nodiscard]] std::string formatTimestampUs(std::int64_t timestampUs, std::string_view format = "%Y-%m-%d %H:%M:%S");

template <typename Rep, typename Period>
[[nodiscard]] inline std::int64_t toNs(const std::chrono::duration<Rep, Period>& d) noexcept
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(d).count();
}

template <typename Rep, typename Period>
[[nodiscard]] inline std::int64_t toUs(const std::chrono::duration<Rep, Period>& d) noexcept
{
    return std::chrono::duration_cast<std::chrono::microseconds>(d).count();
}

template <typename Rep, typename Period>
[[nodiscard]] inline std::int64_t toMs(const std::chrono::duration<Rep, Period>& d) noexcept
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(d).count();
}

template <typename Rep, typename Period>
[[nodiscard]] inline double toSeconds(const std::chrono::duration<Rep, Period>& d) noexcept
{
    return std::chrono::duration<double>(d).count();
}

inline void sleepForNs(std::int64_t ns)
{
    std::this_thread::sleep_for(std::chrono::nanoseconds(ns));
}

inline void sleepForUs(std::int64_t us)
{
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

inline void sleepForMs(std::int64_t ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

template <typename Clock, typename Duration>
inline void sleepUntil(const std::chrono::time_point<Clock, Duration>& tp)
{
    std::this_thread::sleep_until(tp);
}

class Stopwatch
{
    public:
        Stopwatch() noexcept;

        void reset() noexcept;

        [[nodiscard]] std::int64_t elapsedNs() const noexcept;
        [[nodiscard]] std::int64_t elapsedUs() const noexcept;
        [[nodiscard]] std::int64_t elapsedMs() const noexcept;
        [[nodiscard]] double elapsedSeconds() const noexcept;

    private:
        SteadyClock::time_point m_begin;
};

}  // namespace dbase::time