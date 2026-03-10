#pragma once

#include "dbase/net/poller.h"

#include <atomic>
#include <cstdint>
#include <memory>
#include <vector>

namespace dbase::net
{
class Channel;

class EventLoop
{
    public:
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

        void updateChannel(Channel* channel);
        void removeChannel(Channel* channel);

        [[nodiscard]] Poller* poller() noexcept;
        [[nodiscard]] const std::vector<Channel*>& activeChannels() const noexcept;

    private:
        std::atomic<bool> m_looping{false};
        std::atomic<bool> m_quit{false};
        const std::uint64_t m_threadId;
        std::unique_ptr<Poller> m_poller;
        std::vector<Channel*> m_activeChannels;
};
}  // namespace dbase::net