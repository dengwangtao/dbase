#include "dbase/net/channel.h"

#include "dbase/net/event_loop.h"

#include <stdexcept>

namespace dbase::net
{
Channel::Channel(EventLoop* loop, SocketType fd)
    : m_loop(loop),
      m_fd(fd)
{
    if (m_loop == nullptr)
    {
        throw std::invalid_argument("Channel loop is null");
    }

    if (m_fd == kInvalidSocket)
    {
        throw std::invalid_argument("Channel fd is invalid");
    }
}

EventLoop* Channel::ownerLoop() const noexcept
{
    return m_loop;
}

SocketType Channel::fd() const noexcept
{
    return m_fd;
}

std::uint32_t Channel::events() const noexcept
{
    return m_events;
}

std::uint32_t Channel::revents() const noexcept
{
    return m_revents;
}

bool Channel::isNoneEvent() const noexcept
{
    return m_events == kNoneEvent;
}

void Channel::setRevents(std::uint32_t revents) noexcept
{
    m_revents = revents;
}

void Channel::setReadCallback(EventCallback cb)
{
    m_readCallback = std::move(cb);
}

void Channel::setWriteCallback(EventCallback cb)
{
    m_writeCallback = std::move(cb);
}

void Channel::setCloseCallback(EventCallback cb)
{
    m_closeCallback = std::move(cb);
}

void Channel::setErrorCallback(EventCallback cb)
{
    m_errorCallback = std::move(cb);
}

void Channel::enableReading()
{
    m_events |= kReadEvent;
    update();
}

void Channel::enableWriting()
{
    m_events |= kWriteEvent;
    update();
}

void Channel::disableWriting()
{
    m_events &= ~kWriteEvent;
    update();
}

void Channel::disableAll()
{
    m_events = kNoneEvent;
    update();
}

bool Channel::isReading() const noexcept
{
    return (m_events & kReadEvent) != 0;
}

bool Channel::isWriting() const noexcept
{
    return (m_events & kWriteEvent) != 0;
}

void Channel::remove()
{
    m_loop->removeChannel(this);
}

void Channel::handleEvent()
{
    if ((m_revents & kReadEvent) != 0)
    {
        if (m_readCallback)
        {
            m_readCallback();
        }
    }

    if ((m_revents & kWriteEvent) != 0)
    {
        if (m_writeCallback)
        {
            m_writeCallback();
        }
    }

    if ((m_revents & (kReadEvent | kWriteEvent)) == 0)
    {
        if (m_closeCallback)
        {
            m_closeCallback();
        }
    }
}

void Channel::update()
{
    m_loop->updateChannel(this);
}

}  // namespace dbase::net