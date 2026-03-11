#pragma once

#include "dbase/net/poller.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <vector>

namespace dbase::net
{
class Channel;
class WakeupChannel;

class EventLoop
{
    public:
        using Functor = std::function<void()>;

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

        void updateChannel(Channel* channel);
        void removeChannel(Channel* channel);

        [[nodiscard]] Poller* poller() noexcept;
        [[nodiscard]] const std::vector<Channel*>& activeChannels() const noexcept;

    private:
        void wakeup();
        void handleWakeupRead();
        void doPendingFunctors();

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
};
}  // namespace dbase::net