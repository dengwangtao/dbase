#pragma once

#include "dbase/net/poller.h"

#include <unordered_map>
#include <vector>

#if defined(__linux__)
#include <sys/epoll.h>
#endif

namespace dbase::net
{
class EpollPoller final : public Poller
{
    public:
        explicit EpollPoller(EventLoop* loop);
        ~EpollPoller() override;

        std::int64_t poll(int timeoutMs, ChannelList* activeChannels) override;
        void updateChannel(Channel* channel) override;
        void removeChannel(Channel* channel) override;

    private:
#if defined(__linux__)
        void update(int operation, Channel* channel);
        static std::uint32_t toEpollEvents(Channel* channel) noexcept;
        static std::uint32_t fromEpollEvents(std::uint32_t epollEvents) noexcept;
#endif

    private:
#if defined(__linux__)
        int m_epollFd{-1};
        std::vector<epoll_event> m_events;
#endif
        std::unordered_map<SocketType, Channel*> m_channels;
};
}  // namespace dbase::net