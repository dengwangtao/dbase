#pragma once

#include "dbase/net/socket_ops.h"

namespace dbase::net
{
class WakeupChannel
{
    public:
        WakeupChannel();
        ~WakeupChannel();

        WakeupChannel(const WakeupChannel&) = delete;
        WakeupChannel& operator=(const WakeupChannel&) = delete;

        WakeupChannel(WakeupChannel&&) = delete;
        WakeupChannel& operator=(WakeupChannel&&) = delete;

        [[nodiscard]] SocketType readFd() const noexcept;
        [[nodiscard]] SocketType writeFd() const noexcept;

        void wakeup();
        void handleRead();

    private:
        SocketType m_readFd{kInvalidSocket};
        SocketType m_writeFd{kInvalidSocket};
};
}  // namespace dbase::net