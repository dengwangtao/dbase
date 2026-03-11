#pragma once

#include "dbase/net/poller.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>
#include <queue>

namespace dbase::net
{
class Channel;
class WakeupChannel;

class EventLoop
{
    public:
        using Functor = std::function<void()>;
        using TimerId = std::uint64_t;

        EventLoop();
        ~EventLoop();

        EventLoop(const EventLoop&) = delete;
        EventLoop& operator=(const EventLoop&) = delete;

        void loop();
        void quit() noexcept;

        [[nodiscard]] bool looping() const noexcept;
        [[nodiscard]] bool quitRequested() const noexcept;
        [[nodiscard]] std::uint64_t threadId() const noexcept;

        void assertInLoopThread() const;
        [[nodiscard]] bool isInLoopThread() const noexcept;

        void runInLoop(Functor cb);
        void queueInLoop(Functor cb);

        TimerId runAfter(std::chrono::milliseconds delay, Functor cb);
        TimerId runEvery(std::chrono::milliseconds interval, Functor cb);
        void cancelTimer(TimerId timerId);

        void updateChannel(Channel* channel);
        void removeChannel(Channel* channel);

        [[nodiscard]] Poller* poller() noexcept;
        [[nodiscard]] const std::vector<Channel*>& activeChannels() const noexcept;

    private:
        using Clock = std::chrono::steady_clock;
        using TimePoint = Clock::time_point;

        struct TimerTask
        {
                TimerId id{0};
                TimePoint expiration;
                std::chrono::milliseconds interval{0};
                bool repeat{false};
                bool cancelled{false};
                Functor callback;
        };

        struct TimerCompare
        {
                bool operator()(const std::shared_ptr<TimerTask>& lhs, const std::shared_ptr<TimerTask>& rhs) const noexcept
                {
                    if (lhs->expiration != rhs->expiration)
                    {
                        return lhs->expiration > rhs->expiration;
                    }
                    return lhs->id > rhs->id;
                }
        };

    private:
        void wakeup();
        void handleWakeupRead();
        void doPendingFunctors();

        TimerId runAt(TimePoint expiration, std::chrono::milliseconds interval, bool repeat, Functor cb);
        void addTimerInLoop(const std::shared_ptr<TimerTask>& timerTask);
        void cancelTimerInLoop(TimerId timerId);
        void processTimers();
        [[nodiscard]] int getPollTimeoutMs() const noexcept;

    private:
        std::atomic<bool> m_looping{false};
        std::atomic<bool> m_quit{false};
        const std::uint64_t m_threadId;
        std::unique_ptr<Poller> m_poller;
        std::vector<Channel*> m_activeChannels;

        std::unique_ptr<WakeupChannel> m_wakeupChannel;
        std::unique_ptr<Channel> m_wakeupFdChannel;

        mutable std::mutex m_mutex;
        std::vector<Functor> m_pendingFunctors;
        std::atomic<bool> m_callingPendingFunctors{false};

        std::priority_queue<
                std::shared_ptr<TimerTask>,
                std::vector<std::shared_ptr<TimerTask>>,
                TimerCompare>
                m_timers;

        std::unordered_map<TimerId, std::shared_ptr<TimerTask>> m_timerMap;
        std::atomic<TimerId> m_nextTimerId{1};
};
}  // namespace dbase::net