#pragma once

#include "dbase/net/socket_ops.h"

#include <cstdint>
#include <functional>

namespace dbase::net
{
class EventLoop;

class Channel
{
    public:
        using EventCallback = std::function<void()>;

        static constexpr std::uint32_t kNoneEvent = 0;
        static constexpr std::uint32_t kReadEvent = 1u << 0;
        static constexpr std::uint32_t kWriteEvent = 1u << 1;

        Channel(EventLoop* loop, SocketType fd);

        [[nodiscard]] EventLoop* ownerLoop() const noexcept;
        [[nodiscard]] SocketType fd() const noexcept;

        [[nodiscard]] std::uint32_t events() const noexcept;
        [[nodiscard]] std::uint32_t revents() const noexcept;
        [[nodiscard]] bool isNoneEvent() const noexcept;

        void setRevents(std::uint32_t revents) noexcept;

        void setReadCallback(EventCallback cb);
        void setWriteCallback(EventCallback cb);
        void setCloseCallback(EventCallback cb);
        void setErrorCallback(EventCallback cb);

        void enableReading();
        void enableWriting();
        void disableWriting();
        void disableAll();

        [[nodiscard]] bool isReading() const noexcept;
        [[nodiscard]] bool isWriting() const noexcept;

        void remove();
        void handleEvent();

    private:
        void update();

    private:
        EventLoop* m_loop{nullptr};
        SocketType m_fd{kInvalidSocket};
        std::uint32_t m_events{kNoneEvent};
        std::uint32_t m_revents{kNoneEvent};

        EventCallback m_readCallback;
        EventCallback m_writeCallback;
        EventCallback m_closeCallback;
        EventCallback m_errorCallback;
};
}  // namespace dbase::net