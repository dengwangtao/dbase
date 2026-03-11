#pragma once

#include "dbase/net/channel.h"
#include "dbase/net/event_loop.h"
#include "dbase/net/inet_address.h"
#include "dbase/net/socket.h"

#include <functional>
#include <memory>

namespace dbase::net
{
class Connector : public std::enable_shared_from_this<Connector>
{
    public:
        using NewConnectionCallback = std::function<void(Socket)>;

        enum class State
        {
            Disconnected,
            Connecting,
            Connected
        };

        Connector(EventLoop* loop, const InetAddress& serverAddr);

        Connector(const Connector&) = delete;
        Connector& operator=(const Connector&) = delete;

        ~Connector();

        void setNewConnectionCallback(NewConnectionCallback cb);

        void setRetryDelayMs(int initialDelayMs, int maxDelayMs) noexcept;
        [[nodiscard]] int initialRetryDelayMs() const noexcept;
        [[nodiscard]] int maxRetryDelayMs() const noexcept;
        [[nodiscard]] int currentRetryDelayMs() const noexcept;

        void start();
        void stop();
        void restart();

        [[nodiscard]] EventLoop* ownerLoop() const noexcept;
        [[nodiscard]] const InetAddress& serverAddress() const noexcept;
        [[nodiscard]] State state() const noexcept;
        [[nodiscard]] bool started() const noexcept;

    private:
        void startInLoop();
        void stopInLoop();

        void connect();
        void connecting(Socket socket);
        void handleWrite();
        void handleError();
        void retry();

        void cancelRetry();
        void removeAndResetChannel();
        [[nodiscard]] SocketType removeAndResetChannelRaw();

        [[nodiscard]] SocketType createNonblockingSocket() const;
        void setState(State state) noexcept;

    private:
        EventLoop* m_loop{nullptr};
        InetAddress m_serverAddr;
        bool m_started{false};
        State m_state{State::Disconnected};
        std::unique_ptr<Channel> m_channel;
        SocketType m_socket{kInvalidSocket};

        int m_initialRetryDelayMs{500};
        int m_maxRetryDelayMs{30000};
        int m_retryDelayMs{500};

        NewConnectionCallback m_newConnectionCallback;
        EventLoop::TimerId m_retryTimerId{0};
};
}  // namespace dbase::net