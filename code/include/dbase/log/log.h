#pragma once

#include <format>
#include <memory>
#include <mutex>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

namespace dbase::log
{
class Sink;

enum class Level
{
    Trace = 0,
    Debug,
    Info,
    Warn,
    Error,
    Fatal
};

enum class PatternStyle
{
    Compact,
    Source,
    Threaded
};

struct LogEvent
{
        Level level{Level::Info};
        std::string message;
        std::string file;
        std::string function;
        std::uint_least32_t line{0};
        std::uint64_t pid{0};
        std::uint64_t tid{0};
        std::int64_t timestampUs{0};
};

class Formatter
{
    public:
        Formatter() = default;
        explicit Formatter(PatternStyle style);

        void setStyle(PatternStyle style) noexcept;
        [[nodiscard]] PatternStyle style() const noexcept;

        [[nodiscard]] std::string format(const LogEvent& event) const;

    private:
        [[nodiscard]] std::string formatCompact(const LogEvent& event) const;
        [[nodiscard]] std::string formatSource(const LogEvent& event) const;
        [[nodiscard]] std::string formatThreaded(const LogEvent& event) const;

    private:
        PatternStyle m_style{PatternStyle::Source};
};

class Logger
{
    public:
        Logger();
        explicit Logger(PatternStyle style);

        void setLevel(Level level) noexcept;
        [[nodiscard]] Level level() const noexcept;
        [[nodiscard]] bool shouldLog(Level level) const noexcept;

        void setPatternStyle(PatternStyle style) noexcept;
        [[nodiscard]] PatternStyle patternStyle() const noexcept;

        void addSink(std::shared_ptr<Sink> sink);
        void clearSinks();

        void log(Level level, std::string_view message, const std::source_location& location = std::source_location::current());

        template <typename... Args>
        void logf(Level level, const std::source_location& location, std::format_string<Args...> fmt, Args&&... args)
        {
            if (!shouldLog(level))
            {
                return;
            }

            log(level, std::format(fmt, std::forward<Args>(args)...), location);
        }

    private:
        [[nodiscard]] LogEvent buildEvent(Level level, std::string_view message, const std::source_location& location) const;

    private:
        mutable std::mutex m_mutex;
        Level m_level{Level::Info};
        Formatter m_formatter;
        std::vector<std::shared_ptr<Sink>> m_sinks;
};

[[nodiscard]] const char* toString(Level level) noexcept;
[[nodiscard]] const char* toSpdlogString(Level level) noexcept;

Logger& defaultLogger();

void setDefaultLevel(Level level) noexcept;
void setDefaultPatternStyle(PatternStyle style) noexcept;
void addDefaultSink(std::shared_ptr<Sink> sink);
void resetDefaultSinks();

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