#include "dbase/log/async_logger.h"
#include "dbase/log/sink.h"
#include <deque>
#include <utility>

namespace dbase::log
{
AsyncLogger::AsyncLogger(
        PatternStyle style,
        std::size_t queueCapacity,
        AsyncOverflowPolicy overflowPolicy)
    : m_formatter(style),
      m_queueCapacity(queueCapacity == 0 ? 8192 : queueCapacity),
      m_overflowPolicy(overflowPolicy)
{
    m_sinks.emplace_back(std::make_shared<ConsoleSink>());
    m_worker = std::thread(&AsyncLogger::workerLoop, this);
}

AsyncLogger::~AsyncLogger()
{
    stop();
}

void AsyncLogger::setLevel(Level level) noexcept
{
    m_level.store(level, std::memory_order_release);
}

Level AsyncLogger::level() const noexcept
{
    return m_level.load(std::memory_order_acquire);
}

bool AsyncLogger::shouldLog(Level level) const noexcept
{
    return static_cast<int>(level) >= static_cast<int>(m_level.load(std::memory_order_acquire));
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
    m_flushOn.store(level, std::memory_order_release);
}

Level AsyncLogger::flushOn() const noexcept
{
    return m_flushOn.load(std::memory_order_acquire);
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
    return m_droppedCount.load(std::memory_order_acquire);
}

void AsyncLogger::log(
        Level level,
        std::string_view message,
        const std::source_location& location)
{
    if (!shouldLog(level))
    {
        return;
    }

    QueueItem item;
    item.event = detail::makeLogEvent(level, message, location);
    item.sequence = m_nextSequence.fetch_add(1, std::memory_order_acq_rel) + 1;
    (void)enqueueItem(std::move(item));
}

void AsyncLogger::flush()
{
    if (m_stopping.load(std::memory_order_acquire))
    {
        std::vector<std::shared_ptr<Sink>> sinksCopy;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            sinksCopy = m_sinks;
        }

        for (const auto& sink : sinksCopy)
        {
            sink->flush();
        }
        return;
    }

    QueueItem item;
    item.flushOnly = true;
    item.sequence = m_nextSequence.fetch_add(1, std::memory_order_acq_rel) + 1;

    if (!enqueueItem(item))
    {
        std::vector<std::shared_ptr<Sink>> sinksCopy;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            sinksCopy = m_sinks;
        }

        for (const auto& sink : sinksCopy)
        {
            sink->flush();
        }
        return;
    }

    std::unique_lock<std::mutex> lock(m_mutex);
    m_flushCv.wait(lock, [this, target = item.sequence]()
                   { return m_processedSequence.load(std::memory_order_acquire) >= target; });
}

void AsyncLogger::stop()
{
    bool expected = false;
    if (!m_stopping.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
    {
        return;
    }

    m_cv.notify_all();
    m_flushCv.notify_all();

    if (m_worker.joinable())
    {
        m_worker.join();
    }
}

void AsyncLogger::workerLoop()
{
    for (;;)
    {
        std::deque<QueueItem> batch;
        std::vector<std::shared_ptr<Sink>> sinksCopy;
        Formatter formatterCopy;
        const Level flushOnLevel = m_flushOn.load(std::memory_order_acquire);

        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]()
                      { return m_stopping.load(std::memory_order_acquire) || !m_queue.empty(); });

            if (m_stopping.load(std::memory_order_acquire) && m_queue.empty())
            {
                break;
            }

            batch.swap(m_queue);
            sinksCopy = m_sinks;
            formatterCopy = m_formatter;
        }

        m_cv.notify_all();

        for (auto& item : batch)
        {
            if (item.flushOnly)
            {
                for (const auto& sink : sinksCopy)
                {
                    sink->flush();
                }

                m_processedSequence.store(item.sequence, std::memory_order_release);
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

            m_processedSequence.store(item.sequence, std::memory_order_release);
            m_flushCv.notify_all();
        }
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

bool AsyncLogger::enqueueItem(QueueItem item)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    if (m_stopping.load(std::memory_order_acquire))
    {
        return false;
    }

    if (m_overflowPolicy == AsyncOverflowPolicy::Block || item.flushOnly)
    {
        m_cv.wait(lock, [this]()
                  { return m_stopping.load(std::memory_order_acquire) || m_queue.size() < m_queueCapacity; });

        if (m_stopping.load(std::memory_order_acquire))
        {
            return false;
        }

        m_queue.emplace_back(std::move(item));
        lock.unlock();
        m_cv.notify_all();
        return true;
    }

    if (m_queue.size() >= m_queueCapacity)
    {
        m_droppedCount.fetch_add(1, std::memory_order_acq_rel);
        return false;
    }

    m_queue.emplace_back(std::move(item));
    lock.unlock();
    m_cv.notify_all();
    return true;
}
}  // namespace dbase::log