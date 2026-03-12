#pragma once
#include "dbase/log/log.h"
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <source_location>
#include <string_view>
#include <thread>
#include <vector>

namespace dbase::log
{
class Sink;

enum class AsyncOverflowPolicy
{
    Block,
    DiscardNewest
};

class AsyncLogger
{
    public:
        AsyncLogger(
                PatternStyle style = PatternStyle::Source,
                std::size_t queueCapacity = 8192,
                AsyncOverflowPolicy overflowPolicy = AsyncOverflowPolicy::Block);
        ~AsyncLogger();

        AsyncLogger(const AsyncLogger&) = delete;
        AsyncLogger& operator=(const AsyncLogger&) = delete;

        void setLevel(Level level) noexcept;
        [[nodiscard]] Level level() const noexcept;
        [[nodiscard]] bool shouldLog(Level level) const noexcept;

        void setPatternStyle(PatternStyle style) noexcept;
        [[nodiscard]] PatternStyle patternStyle() const noexcept;

        void setFlushOn(Level level) noexcept;
        [[nodiscard]] Level flushOn() const noexcept;

        void addSink(std::shared_ptr<Sink> sink);
        void clearSinks();

        [[nodiscard]] std::size_t queueCapacity() const noexcept;
        [[nodiscard]] std::size_t droppedCount() const noexcept;

        void log(
                Level level,
                std::string_view message,
                const std::source_location& location = std::source_location::current());

        template <typename... Args>
        void logf(
                Level level,
                const std::source_location& location,
                std::format_string<Args...>
                        fmt,
                Args&&... args)
        {
            if (!shouldLog(level))
            {
                return;
            }
            log(level, std::format(fmt, std::forward<Args>(args)...), location);
        }

        void flush();
        void stop();

    private:
        struct QueueItem
        {
                LogEvent event;
                std::uint64_t sequence{0};
                bool flushOnly{false};
        };

    private:
        void workerLoop();
        [[nodiscard]] bool enqueueItem(QueueItem item);

    private:
        mutable std::mutex m_mutex;
        std::condition_variable m_cv;
        std::condition_variable m_flushCv;
        std::deque<QueueItem> m_queue;

        std::atomic<Level> m_level{Level::Info};
        std::atomic<Level> m_flushOn{Level::Error};

        Formatter m_formatter;
        std::vector<std::shared_ptr<Sink>> m_sinks;

        std::size_t m_queueCapacity{0};
        AsyncOverflowPolicy m_overflowPolicy{AsyncOverflowPolicy::Block};

        std::atomic<std::size_t> m_droppedCount{0};
        std::atomic<std::uint64_t> m_nextSequence{0};
        std::atomic<std::uint64_t> m_processedSequence{0};
        std::atomic<bool> m_stopping{false};

        std::thread m_worker;
};
}  // namespace dbase::log