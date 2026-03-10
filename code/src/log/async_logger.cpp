#include "dbase/log/async_logger.h"

#include "dbase/log/sink.h"
#include "dbase/platform/process.h"
#include "dbase/time/time.h"

#include <filesystem>
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
}  // namespace

AsyncLogger::AsyncLogger(
        PatternStyle style,
        std::size_t queueCapacity,
        AsyncOverflowPolicy overflowPolicy)
    : m_formatter(style),
      m_queueCapacity(queueCapacity),
      m_overflowPolicy(overflowPolicy)
{
    if (m_queueCapacity == 0)
    {
        m_queueCapacity = 8192;
    }

    m_sinks.emplace_back(std::make_shared<ConsoleSink>());
    m_worker = std::thread(&AsyncLogger::workerLoop, this);
}

AsyncLogger::~AsyncLogger()
{
    stop();
}

void AsyncLogger::setLevel(Level level) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_level = level;
}

Level AsyncLogger::level() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_level;
}

bool AsyncLogger::shouldLog(Level level) const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(level) >= static_cast<int>(m_level);
}

void AsyncLogger::setPatternStyle(PatternStyle style) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_formatter.setStyle(style);
}

PatternStyle AsyncLogger::patternStyle() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_formatter.style();
}

void AsyncLogger::setFlushOn(Level level) noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_flushOn = level;
}

Level AsyncLogger::flushOn() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_flushOn;
}

void AsyncLogger::addSink(std::shared_ptr<Sink> sink)
{
    if (!sink)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_sinks.emplace_back(std::move(sink));
}

void AsyncLogger::clearSinks()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_sinks.clear();
}

std::size_t AsyncLogger::queueCapacity() const noexcept
{
    return m_queueCapacity;
}

std::size_t AsyncLogger::droppedCount() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_droppedCount;
}

void AsyncLogger::log(
        Level level,
        std::string_view message,
        const std::source_location& location)
{
    QueueItem item;
    item.event = buildEvent(level, message, location);

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        item.sequence = ++m_nextSequence;
    }

    enqueueItem(std::move(item));
}

void AsyncLogger::flush()
{
    QueueItem item;
    item.flushOnly = true;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        item.sequence = ++m_nextSequence;
    }

    enqueueItem(item);

    std::unique_lock<std::mutex> lock(m_mutex);
    m_flushCv.wait(lock, [this, target = item.sequence]()
                   { return m_processedSequence >= target; });
}

void AsyncLogger::stop()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_stopping)
        {
            return;
        }
        m_stopping = true;
    }

    m_cv.notify_all();

    if (m_worker.joinable())
    {
        m_worker.join();
    }
}

LogEvent AsyncLogger::buildEvent(
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

void AsyncLogger::workerLoop()
{
    for (;;)
    {
        QueueItem item;
        std::vector<std::shared_ptr<Sink>> sinksCopy;
        Formatter formatterCopy;
        Level flushOnLevel{Level::Fatal};

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]()
                      { return m_stopping || !m_queue.empty(); });

            if (m_stopping && m_queue.empty())
            {
                break;
            }

            item = std::move(m_queue.front());
            m_queue.pop_front();

            sinksCopy = m_sinks;
            formatterCopy = m_formatter;
            flushOnLevel = m_flushOn;

            m_cv.notify_all();
        }

        if (item.flushOnly)
        {
            for (const auto& sink : sinksCopy)
            {
                sink->flush();
            }

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_processedSequence = item.sequence;
            }
            m_flushCv.notify_all();
            continue;
        }

        const auto formatted = formatterCopy.format(item.event);

        for (const auto& sink : sinksCopy)
        {
            sink->write(item.event, formatted);
        }

        if (static_cast<int>(item.event.level) >= static_cast<int>(flushOnLevel))
        {
            for (const auto& sink : sinksCopy)
            {
                sink->flush();
            }
        }

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_processedSequence = item.sequence;
        }
        m_flushCv.notify_all();
    }

    std::vector<std::shared_ptr<Sink>> sinksCopy;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        sinksCopy = m_sinks;
    }

    for (const auto& sink : sinksCopy)
    {
        sink->flush();
    }
}

void AsyncLogger::enqueueItem(QueueItem item)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    if (m_stopping)
    {
        return;
    }

    if (m_overflowPolicy == AsyncOverflowPolicy::Block)
    {
        m_cv.wait(lock, [this]()
                  { return m_stopping || m_queue.size() < m_queueCapacity; });

        if (m_stopping)
        {
            return;
        }

        m_queue.emplace_back(std::move(item));
        lock.unlock();
        m_cv.notify_all();
        return;
    }

    if (m_queue.size() >= m_queueCapacity)
    {
        ++m_droppedCount;
        return;
    }

    m_queue.emplace_back(std::move(item));
    lock.unlock();
    m_cv.notify_all();
}

}  // namespace dbase::log