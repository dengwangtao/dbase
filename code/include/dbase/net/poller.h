#pragma once

#include "dbase/net/socket_ops.h"

#include <cstdint>
#include <vector>

namespace dbase::net
{
class Channel;
class EventLoop;

class Poller
{
    public:
        using ChannelList = std::vector<Channel*>;

        explicit Poller(EventLoop* loop);
        virtual ~Poller() = default;

        Poller(const Poller&) = delete;
        Poller& operator=(const Poller&) = delete;

        virtual std::int64_t poll(int timeoutMs, ChannelList* activeChannels) = 0;
        virtual void updateChannel(Channel* channel) = 0;
        virtual void removeChannel(Channel* channel) = 0;

    protected:
        EventLoop* m_ownerLoop{nullptr};
};
}  // namespace dbase::net