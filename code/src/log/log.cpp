#include "dbase/log/log.h"

#include "dbase/time/time.h"

#include <filesystem>
#include <format>
#include <iostream>
#include <string>

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

std::string nowText()
{
    const auto us = dbase::time::nowUs();
    const auto ms = static_cast<int>((us / 1000) % 1000);
    return std::format("{}.{:03}", dbase::time::formatNow("%Y-%m-%d %H:%M:%S"), ms);
}

std::string levelText(Level level)
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

std::string buildPrefix(Level level, const std::source_location& location)
{
    return std::format(
            "[{}] [{}] [{}:{}] [{}] ",
            nowText(),
            levelText(level),
            baseFileName(location.file_name()),
            location.line(),
            normalizeFunctionName(location.function_name()));
}
}  // namespace

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

void Logger::log(
        Level level,
        std::string_view message,
        const std::source_location& location)
{
    if (!shouldLog(level))
    {
        return;
    }

    const auto text = buildPrefix(level, location) + std::string(message);

    std::lock_guard<std::mutex> lock(m_mutex);

    if (static_cast<int>(level) >= static_cast<int>(Level::Error))
    {
        std::cerr << text << std::endl;
        return;
    }

    std::cout << text << std::endl;
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

Logger& defaultLogger()
{
    static Logger logger;
    return logger;
}

void setDefaultLevel(Level level) noexcept
{
    defaultLogger().setLevel(level);
}

void log(
        Level level,
        std::string_view message,
        const std::source_location& location)
{
    defaultLogger().log(level, message, location);
}

}  // namespace dbase::log