#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

namespace dbase::time
{
using SteadyClock = std::chrono::steady_clock;
using SystemClock = std::chrono::system_clock;

[[nodiscard]] std::int64_t nowMs();
[[nodiscard]] std::int64_t nowUs();
[[nodiscard]] std::int64_t steadyNowMs();

[[nodiscard]] std::string formatNow(std::string_view format = "%Y-%m-%d %H:%M:%S");
[[nodiscard]] std::string formatTimestampMs(std::int64_t timestampMs, std::string_view format = "%Y-%m-%d %H:%M:%S");

class Stopwatch
{
    public:
        Stopwatch();

        void reset() noexcept;

        [[nodiscard]] std::int64_t elapsedMs() const noexcept;
        [[nodiscard]] std::int64_t elapsedUs() const noexcept;

    private:
        SteadyClock::time_point m_begin;
};

}  // namespace dbase::time