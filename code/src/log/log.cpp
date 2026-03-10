#include "dbase/log/log.h"

#include "dbase/log/sink.h"
#include "dbase/platform/process.h"
#include "dbase/time/time.h"

#include <filesystem>
#include <format>
#include <string>
#include <utility>

namespace dbase::log
{
namespace
{
std::string baseFileName(std::string_view file)
{
    return std::filesystem::path(file).filename().string();
}

void replaceAllInPlace(std::string& text, std::string_view from, std::string_view to)
{
    if (from.empty())
    {
        return;
    }

    std::size_t pos = 0;
    while ((pos = text.find(from.data(), pos, from.size())) != std::string::npos)
    {
        text.replace(pos, from.size(), to.data(), to.size());
        pos += to.size();
    }
}

std::string normalizeFunctionName(std::string_view func)
{
    std::string text(func);

    replaceAllInPlace(text, "__cdecl ", "");
    replaceAllInPlace(text, "__thiscall ", "");
    replaceAllInPlace(text, "__vectorcall ", "");
    replaceAllInPlace(text, "__stdcall ", "");
    replaceAllInPlace(text, "__fastcall ", "");
    replaceAllInPlace(text, "(void)", "()");

    const auto parenPos = text.find('(');
    if (parenPos != std::string::npos)
    {
        const auto spacePos = text.rfind(' ', parenPos);
        if (spacePos != std::string::npos)
        {
            text.erase(0, spacePos + 1);
        }
    }

    return text;
}

std::string formatTimestampUs(std::int64_t timestampUs)
{
    const auto ms = static_cast<int>((timestampUs / 1000) % 1000);
    return std::format(
            "{}.{:03}",
            dbase::time::formatTimestampMs(timestampUs / 1000, "%Y-%m-%d %H:%M:%S"),
            ms);
}
}  // namespace

Formatter::Formatter(PatternStyle style)
    : m_style(style)
{
}

void Formatter::setStyle(PatternStyle style) noexcept
{
    m_style = style;
}

PatternStyle Formatter::style() const noexcept
{
    return m_style;
}

std::string Formatter::format(const LogEvent& event) const
{
    switch (m_style)
    {
        case PatternStyle::Compact:
            return formatCompact(event);
        case PatternStyle::Source:
            return formatSource(event);
        case PatternStyle::Threaded:
            return formatThreaded(event);
        default:
            return formatSource(event);
    }
}

std::string Formatter::formatCompact(const LogEvent& event) const
{
    return std::format(
            "[{}] [{}] {}",
            formatTimestampUs(event.timestampUs),
            toSpdlogString(event.level),
            event.message);
}

std::string Formatter::formatSource(const LogEvent& event) const
{
    return std::format(
            "[{}] [{}] [{}:{}] [{}] {}",
            formatTimestampUs(event.timestampUs),
            toSpdlogString(event.level),
            event.file,
            event.line,
            event.function,
            event.message);
}

std::string Formatter::formatThreaded(const LogEvent& event) const
{
    return std::format(
            "[{}] [{}] [{}:{}] [{}:{}] [{}] {}",
            formatTimestampUs(event.timestampUs),
            toSpdlogString(event.level),
            event.pid,
            event.tid,
            event.file,
            event.line,
            event.function,
            event.message);
}

Logger::Logger()
    : Logger(PatternStyle::Source)
{
}

Logger::Logger(PatternStyle style)
    : m_formatter(style)
{
    m_sinks.emplace_back(std::make_shared<ConsoleSink>());
}

void Logger::setLevel(Level level) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_level = level;
}

Level Logger::level() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_level;
}

bool Logger::shouldLog(Level level) const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(level) >= static_cast<int>(m_level);
}

void Logger::setPatternStyle(PatternStyle style) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_formatter.setStyle(style);
}

PatternStyle Logger::patternStyle() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_formatter.style();
}

void Logger::addSink(std::shared_ptr<Sink> sink)
{
    if (!sink)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_sinks.emplace_back(std::move(sink));
}

void Logger::clearSinks()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sinks.clear();
}

LogEvent Logger::buildEvent(
        Level level,
        std::string_view message,
        const std::source_location& location) const
{
    LogEvent event;
    event.level = level;
    event.message = std::string(message);
    event.file = baseFileName(location.file_name());
    event.function = normalizeFunctionName(location.function_name());
    event.line = location.line();
    event.pid = dbase::platform::pid();
    event.tid = dbase::platform::tid();
    event.timestampUs = dbase::time::nowUs();
    return event;
}

void Logger::log(
        Level level,
        std::string_view message,
        const std::source_location& location)
{
    std::vector<std::shared_ptr<Sink>> sinksCopy;
    Formatter formatterCopy;

    {
        std::lock_guard<std::mutex> lock(m_mutex);

        if (static_cast<int>(level) < static_cast<int>(m_level))
        {
            return;
        }

        sinksCopy = m_sinks;
        formatterCopy = m_formatter;
    }

    const auto event = buildEvent(level, message, location);
    const auto formatted = formatterCopy.format(event);

    for (const auto& sink : sinksCopy)
    {
        sink->write(event, formatted);
    }
}

const char* toString(Level level) noexcept
{
    switch (level)
    {
        case Level::Trace:
            return "TRACE";
        case Level::Debug:
            return "DEBUG";
        case Level::Info:
            return "INFO";
        case Level::Warn:
            return "WARN";
        case Level::Error:
            return "ERROR";
        case Level::Fatal:
            return "FATAL";
        default:
            return "UNKNOWN";
    }
}

const char* toSpdlogString(Level level) noexcept
{
    switch (level)
    {
        case Level::Trace:
            return "trace";
        case Level::Debug:
            return "debug";
        case Level::Info:
            return "info";
        case Level::Warn:
            return "warn";
        case Level::Error:
            return "error";
        case Level::Fatal:
            return "critical";
        default:
            return "unknown";
    }
}

Logger& defaultLogger()
{
    static Logger logger;
    return logger;
}

void setDefaultLevel(Level level) noexcept
{
    defaultLogger().setLevel(level);
}

void setDefaultPatternStyle(PatternStyle style) noexcept
{
    defaultLogger().setPatternStyle(style);
}

void addDefaultSink(std::shared_ptr<Sink> sink)
{
    defaultLogger().addSink(std::move(sink));
}

void resetDefaultSinks()
{
    auto& logger = defaultLogger();
    logger.clearSinks();
    logger.addSink(std::make_shared<ConsoleSink>());
}

void log(
        Level level,
        std::string_view message,
        const std::source_location& location)
{
    defaultLogger().log(level, message, location);
}

}  // namespace dbase::log