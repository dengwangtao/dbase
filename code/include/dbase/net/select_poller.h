#pragma once

#include "dbase/net/poller.h"

#include <unordered_map>

namespace dbase::net
{
class SelectPoller final : public Poller
{
    public:
        explicit SelectPoller(EventLoop* loop);

        std::int64_t poll(int timeoutMs, ChannelList* activeChannels) override;
        void updateChannel(Channel* channel) override;
        void removeChannel(Channel* channel) override;

    private:
        std::unordered_map<SocketType, Channel*> m_channels;
};
}  // namespace dbase::net