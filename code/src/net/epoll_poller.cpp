#include "dbase/net/epoll_poller.h"

#include "dbase/net/channel.h"
#include "dbase/net/socket_ops.h"
#include "dbase/time/time.h"

#include <stdexcept>
#include <string>

#if defined(__linux__)

#include <cerrno>
#include <cstring>
#include <unistd.h>

namespace dbase::net
{
namespace
{
constexpr int kInitEventListSize = 16;
}

EpollPoller::EpollPoller(EventLoop* loop)
    : Poller(loop),
      m_epollFd(::epoll_create1(EPOLL_CLOEXEC)),
      m_events(kInitEventListSize)
{
    if (m_epollFd < 0)
    {
        throw std::runtime_error("EpollPoller epoll_create1 failed: " + SocketOps::lastErrorMessage());
    }
}

EpollPoller::~EpollPoller()
{
    if (m_epollFd >= 0)
    {
        ::close(m_epollFd);
        m_epollFd = -1;
    }
}

std::int64_t EpollPoller::poll(int timeoutMs, ChannelList* activeChannels)
{
    if (activeChannels == nullptr)
    {
        throw std::invalid_argument("EpollPoller::poll activeChannels is null");
    }

    activeChannels->clear();

    const auto beginUs = dbase::time::nowUs();

    const int numEvents = ::epoll_wait(
            m_epollFd,
            m_events.data(),
            static_cast<int>(m_events.size()),
            timeoutMs);

    if (numEvents < 0)
    {
        if (errno == EINTR)
        {
            return dbase::time::nowUs() - beginUs;
        }

        throw std::runtime_error("EpollPoller::poll epoll_wait failed: " + std::string(std::strerror(errno)));
    }

    for (int i = 0; i < numEvents; ++i)
    {
        auto* channel = static_cast<Channel*>(m_events[static_cast<std::size_t>(i)].data.ptr);
        if (channel == nullptr)
        {
            continue;
        }

        channel->setRevents(fromEpollEvents(m_events[static_cast<std::size_t>(i)].events));
        activeChannels->push_back(channel);
    }

    if (static_cast<std::size_t>(numEvents) == m_events.size())
    {
        m_events.resize(m_events.size() * 2);
    }

    return dbase::time::nowUs() - beginUs;
}

void EpollPoller::updateChannel(Channel* channel)
{
    if (channel == nullptr)
    {
        throw std::invalid_argument("EpollPoller::updateChannel channel is null");
    }

    const auto fd = channel->fd();
    const auto it = m_channels.find(fd);

    if (channel->isNoneEvent())
    {
        if (it != m_channels.end())
        {
            update(EPOLL_CTL_DEL, channel);
            m_channels.erase(it);
        }
        return;
    }

    if (it == m_channels.end())
    {
        m_channels.emplace(fd, channel);
        update(EPOLL_CTL_ADD, channel);
        return;
    }

    update(EPOLL_CTL_MOD, channel);
}

void EpollPoller::removeChannel(Channel* channel)
{
    if (channel == nullptr)
    {
        return;
    }

    const auto it = m_channels.find(channel->fd());
    if (it == m_channels.end())
    {
        return;
    }

    update(EPOLL_CTL_DEL, channel);
    m_channels.erase(it);
}

void EpollPoller::update(int operation, Channel* channel)
{
    epoll_event ev;
    std::memset(&ev, 0, sizeof(ev));
    ev.events = toEpollEvents(channel);
    ev.data.ptr = channel;

    if (::epoll_ctl(m_epollFd, operation, channel->fd(), &ev) != 0)
    {
        throw std::runtime_error("EpollPoller::update epoll_ctl failed: " + std::string(std::strerror(errno)));
    }
}

std::uint32_t EpollPoller::toEpollEvents(Channel* channel) noexcept
{
    std::uint32_t events = 0;

    if (channel->isReading())
    {
        events |= static_cast<std::uint32_t>(EPOLLIN | EPOLLPRI | EPOLLRDHUP);
    }

    if (channel->isWriting())
    {
        events |= static_cast<std::uint32_t>(EPOLLOUT);
    }

    return events;
}

std::uint32_t EpollPoller::fromEpollEvents(std::uint32_t epollEvents) noexcept
{
    std::uint32_t revents = Channel::kNoneEvent;

    if ((epollEvents & (EPOLLERR)) != 0)
    {
        revents |= Channel::kErrorEvent;
    }

    if ((epollEvents & (EPOLLIN | EPOLLPRI)) != 0)
    {
        revents |= Channel::kReadEvent;
    }

    if ((epollEvents & EPOLLOUT) != 0)
    {
        revents |= Channel::kWriteEvent;
    }

    if ((epollEvents & (EPOLLRDHUP | EPOLLHUP)) != 0)
    {
        revents |= Channel::kCloseEvent;
    }

    return revents;
}

}  // namespace dbase::net

#else

namespace dbase::net
{
EpollPoller::EpollPoller(EventLoop* loop)
    : Poller(loop)
{
    throw std::runtime_error("EpollPoller is only supported on Linux");
}

EpollPoller::~EpollPoller() = default;

std::int64_t EpollPoller::poll(int, ChannelList*)
{
    throw std::runtime_error("EpollPoller is only supported on Linux");
}

void EpollPoller::updateChannel(Channel*)
{
    throw std::runtime_error("EpollPoller is only supported on Linux");
}

void EpollPoller::removeChannel(Channel*)
{
    throw std::runtime_error("EpollPoller is only supported on Linux");
}
}  // namespace dbase::net

#endif