#include "dbase/net/select_poller.h"

#include "dbase/net/channel.h"
#include "dbase/net/event_loop.h"
#include "dbase/time/time.h"

#include <stdexcept>

#if defined(_WIN32)
#include <WinSock2.h>
#else
#include <sys/select.h>
#endif

namespace dbase::net
{
SelectPoller::SelectPoller(EventLoop* loop)
    : Poller(loop)
{
}

std::int64_t SelectPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
    if (activeChannels == nullptr)
    {
        throw std::invalid_argument("SelectPoller::poll activeChannels is null");
    }

    activeChannels->clear();

    fd_set readSet;
    fd_set writeSet;
    fd_set exceptSet;

    FD_ZERO(&readSet);
    FD_ZERO(&writeSet);
    FD_ZERO(&exceptSet);

    SocketType maxFd = 0;

    for (const auto& [fd, channel] : m_channels)
    {
        if (channel->isReading())
        {
            FD_SET(fd, &readSet);
        }

        if (channel->isWriting())
        {
            FD_SET(fd, &writeSet);
        }

        FD_SET(fd, &exceptSet);

#if !defined(_WIN32)
        if (fd > maxFd)
        {
            maxFd = fd;
        }
#endif
    }

    timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    const auto beginUs = dbase::time::nowUs();

#if defined(_WIN32)
    const int readyCount = ::select(0, &readSet, &writeSet, &exceptSet, &tv);
#else
    const int readyCount = ::select(static_cast<int>(maxFd + 1), &readSet, &writeSet, &exceptSet, &tv);
#endif

    if (readyCount < 0)
    {
        throw std::runtime_error("SelectPoller::poll select failed: " + SocketOps::lastErrorMessage());
    }

    if (readyCount == 0)
    {
        return dbase::time::nowUs() - beginUs;
    }

    for (const auto& [fd, channel] : m_channels)
    {
        std::uint32_t revents = Channel::kNoneEvent;

        if (FD_ISSET(fd, &readSet))
        {
            revents |= Channel::kReadEvent;
        }

        if (FD_ISSET(fd, &writeSet))
        {
            revents |= Channel::kWriteEvent;
        }

        if (FD_ISSET(fd, &exceptSet))
        {
            if (revents == Channel::kNoneEvent)
            {
                revents = Channel::kNoneEvent;
            }
        }

        if (revents != Channel::kNoneEvent)
        {
            channel->setRevents(revents);
            activeChannels->push_back(channel);
        }
    }

    return dbase::time::nowUs() - beginUs;
}

void SelectPoller::updateChannel(Channel* channel)
{
    if (channel == nullptr)
    {
        throw std::invalid_argument("SelectPoller::updateChannel channel is null");
    }

    if (channel->isNoneEvent())
    {
        m_channels.erase(channel->fd());
        return;
    }

    m_channels[channel->fd()] = channel;
}

void SelectPoller::removeChannel(Channel* channel)
{
    if (channel == nullptr)
    {
        return;
    }

    m_channels.erase(channel->fd());
}

}  // namespace dbase::net