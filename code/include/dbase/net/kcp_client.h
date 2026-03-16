#pragma once

#include "dbase/net/channel.h"
#include "dbase/net/event_loop.h"
#include "dbase/net/inet_address.h"
#include "dbase/net/kcp_session.h"
#include "dbase/net/udp_socket.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>

namespace dbase::net
{
class KcpClient
{
    public:
        using SessionPtr = KcpSession::Ptr;
        using ConnectionCallback = std::function<void(const SessionPtr&)>;
        using MessageCallback = KcpSession::MessageCallback;
        using WriteCompleteCallback = KcpSession::WriteCompleteCallback;
        using CloseCallback = KcpSession::CloseCallback;
        using ErrorCallback = KcpSession::ErrorCallback;

        KcpClient(
                EventLoop* loop,
                const InetAddress& serverAddr,
                std::string name,
                KcpSession::Options options);

        KcpClient(const KcpClient&) = delete;
        KcpClient& operator=(const KcpClient&) = delete;
        ~KcpClient();

        void setConnectionCallback(ConnectionCallback cb);
        void setMessageCallback(MessageCallback cb);
        void setWriteCompleteCallback(WriteCompleteCallback cb);
        void setCloseCallback(CloseCallback cb);
        void setErrorCallback(ErrorCallback cb);
        void enableEdgeTriggered(bool on) noexcept;

        void connect();
        void disconnect();
        void stop();

        [[nodiscard]] EventLoop* ownerLoop() const noexcept;
        [[nodiscard]] const std::string& name() const noexcept;
        [[nodiscard]] const InetAddress& serverAddress() const noexcept;
        [[nodiscard]] SessionPtr session() const;

    private:
        void handleRead();
        void handleSessionClose(const SessionPtr& session);

    private:
        EventLoop* m_loop{nullptr};
        std::string m_name;
        InetAddress m_serverAddr;
        KcpSession::Options m_options;
        UdpSocket m_socket;
        std::unique_ptr<Channel> m_channel;
        bool m_started{false};
        bool m_edgeTriggered{false};
        ConnectionCallback m_connectionCallback;
        MessageCallback m_messageCallback;
        WriteCompleteCallback m_writeCompleteCallback;
        CloseCallback m_closeCallback;
        ErrorCallback m_errorCallback;
        mutable std::mutex m_mutex;
        SessionPtr m_session;
};
}  // namespace dbase::net