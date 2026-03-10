#pragma once

#include "dbase/platform/process.h"

#include <format>
#include <memory>
#include <mutex>
#include <source_location>
#include <string>
#include <string_view>

namespace dbase::log
{
enum class Level
{
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

class Logger
{
    public:
        Logger() = default;

        void setLevel(Level level) noexcept;
        [[nodiscard]] Level level() const noexcept;

        [[nodiscard]] bool shouldLog(Level level) const noexcept;

        void log(Level level, std::string_view message,
                 const std::source_location& location = std::source_location::current());

        template <typename... Args>
        void logf(Level level, std::format_string<Args...> fmt, Args&&... args)
        {
            if (!shouldLog(level))
            {
                return;
            }

            log(level, std::format(fmt, std::forward<Args>(args)...), std::source_location::current());
        }

    private:
        mutable std::mutex m_mutex;
        Level m_level{Level::Info};
};

[[nodiscard]] const char* toString(Level level) noexcept;

Logger& defaultLogger();
void setDefaultLevel(Level level) noexcept;

void log(Level level, std::string_view message, const std::source_location& location = std::source_location::current());

template <typename... Args>
void logf(Level level, const std::source_location& location, std::format_string<Args...> fmt, Args&&... args)
{
    if (!defaultLogger().shouldLog(level))
    {
        return;
    }

    defaultLogger().log(level, std::format(fmt, std::forward<Args>(args)...), location);
}

}  // namespace dbase::log

#define DBASE_LOG_TRACE(...) \
    ::dbase::log::logf(::dbase::log::Level::Trace, std::source_location::current(), __VA_ARGS__)

#define DBASE_LOG_DEBUG(...) \
    ::dbase::log::logf(::dbase::log::Level::Debug, std::source_location::current(), __VA_ARGS__)

#define DBASE_LOG_INFO(...) \
    ::dbase::log::logf(::dbase::log::Level::Info, std::source_location::current(), __VA_ARGS__)

#define DBASE_LOG_WARN(...) \
    ::dbase::log::logf(::dbase::log::Level::Warn, std::source_location::current(), __VA_ARGS__)

#define DBASE_LOG_ERROR(...) \
    ::dbase::log::logf(::dbase::log::Level::Error, std::source_location::current(), __VA_ARGS__)

#define DBASE_LOG_FATAL(...) \
    ::dbase::log::logf(::dbase::log::Level::Fatal, std::source_location::current(), __VA_ARGS__)