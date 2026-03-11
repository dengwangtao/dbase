#pragma once

#include "dbase/net/socket_ops.h"

#include <cstdint>
#include <functional>
#include <memory>

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
        static constexpr std::uint32_t kErrorEvent = 1u << 2;
        static constexpr std::uint32_t kCloseEvent = 1u << 3;

        Channel(EventLoop* loop, SocketType fd);

        [[nodiscard]] EventLoop* ownerLoop() const noexcept;
        [[nodiscard]] SocketType fd() const noexcept;

        [[nodiscard]] std::uint32_t events() const noexcept;
        [[nodiscard]] std::uint32_t revents() const noexcept;
        [[nodiscard]] bool isNoneEvent() const noexcept;

        [[nodiscard]] bool edgeTriggered() const noexcept;
        void setEdgeTriggered(bool on);

        void setRevents(std::uint32_t revents) noexcept;

        void setReadCallback(EventCallback cb);
        void setWriteCallback(EventCallback cb);
        void setCloseCallback(EventCallback cb);
        void setErrorCallback(EventCallback cb);

        void tie(const std::shared_ptr<void>& obj);

        void enableReading();
        void enableWriting();
        void disableReading();
        void disableWriting();
        void disableAll();

        [[nodiscard]] bool isReading() const noexcept;
        [[nodiscard]] bool isWriting() const noexcept;

        void remove();
        void handleEvent();

    private:
        void update();
        void handleEventWithGuard();

    private:
        EventLoop* m_loop{nullptr};
        SocketType m_fd{kInvalidSocket};
        std::uint32_t m_events{kNoneEvent};
        std::uint32_t m_revents{kNoneEvent};
        bool m_edgeTriggered{false};

        EventCallback m_readCallback;
        EventCallback m_writeCallback;
        EventCallback m_closeCallback;
        EventCallback m_errorCallback;

        std::weak_ptr<void> m_tie;
        bool m_tied{false};
};
}  // namespace dbase::net