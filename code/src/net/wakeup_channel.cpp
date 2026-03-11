#include "dbase/net/wakeup_channel.h"

#include "dbase/net/inet_address.h"

#include <stdexcept>

#if defined(_WIN32)
#include <WinSock2.h>
#else
#include <cerrno>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace dbase::net
{
namespace
{
void setNonBlockOrThrow(SocketType fd)
{
    auto ret = SocketOps::setNonBlock(fd, true);
    if (!ret)
    {
        throw std::runtime_error("WakeupChannel setNonBlock failed: " + ret.error().message());
    }
}

#if defined(_WIN32)
void createWakeupSocketPair(SocketType& readFd, SocketType& writeFd)
{
    SocketType listenFd = SocketOps::createTcpNonblockingOrDie(AF_INET);

    auto reuseRet = SocketOps::setReuseAddr(listenFd, true);
    if (!reuseRet)
    {
        SocketOps::close(listenFd);
        throw std::runtime_error("WakeupChannel setReuseAddr failed: " + reuseRet.error().message());
    }

    InetAddress listenAddr(0, true, false);

    auto bindRet = SocketOps::bind(listenFd, listenAddr);
    if (!bindRet)
    {
        SocketOps::close(listenFd);
        throw std::runtime_error("WakeupChannel bind failed: " + bindRet.error().message());
    }

    auto localRet = SocketOps::localAddress(listenFd);
    if (!localRet)
    {
        SocketOps::close(listenFd);
        throw std::runtime_error("WakeupChannel localAddress failed: " + localRet.error().message());
    }

    auto listenRet = SocketOps::listen(listenFd, 1);
    if (!listenRet)
    {
        SocketOps::close(listenFd);
        throw std::runtime_error("WakeupChannel listen failed: " + listenRet.error().message());
    }

    writeFd = SocketOps::createTcpNonblockingOrDie(AF_INET);

    auto connectRet = SocketOps::connect(writeFd, localRet.value());
    if (!connectRet)
    {
        SocketOps::close(writeFd);
        SocketOps::close(listenFd);
        throw std::runtime_error("WakeupChannel connect failed: " + connectRet.error().message());
    }

    for (int i = 0; i < 100; ++i)
    {
        InetAddress peerAddr;
        auto acceptRet = SocketOps::accept(listenFd, &peerAddr);
        if (acceptRet)
        {
            readFd = acceptRet.value();
            SocketOps::close(listenFd);
            setNonBlockOrThrow(readFd);
            setNonBlockOrThrow(writeFd);
            return;
        }

        const int err = WSAGetLastError();
        if (err != WSAEWOULDBLOCK)
        {
            SocketOps::close(writeFd);
            SocketOps::close(listenFd);
            throw std::runtime_error("WakeupChannel accept failed: " + SocketOps::lastErrorMessage());
        }

        Sleep(1);
    }

    SocketOps::close(writeFd);
    SocketOps::close(listenFd);
    throw std::runtime_error("WakeupChannel accept timeout");
}
#else
void createWakeupSocketPair(SocketType& readFd, SocketType& writeFd)
{
    int fds[2] = {-1, -1};
    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, fds) != 0)
    {
        throw std::runtime_error("WakeupChannel socketpair failed: " + SocketOps::lastErrorMessage());
    }

    readFd = fds[0];
    writeFd = fds[1];

    setNonBlockOrThrow(readFd);
    setNonBlockOrThrow(writeFd);
}
#endif
}  // namespace

WakeupChannel::WakeupChannel()
{
    createWakeupSocketPair(m_readFd, m_writeFd);
}

WakeupChannel::~WakeupChannel()
{
    if (m_readFd != kInvalidSocket)
    {
        SocketOps::close(m_readFd);
    }

    if (m_writeFd != kInvalidSocket)
    {
        SocketOps::close(m_writeFd);
    }
}

SocketType WakeupChannel::readFd() const noexcept
{
    return m_readFd;
}

SocketType WakeupChannel::writeFd() const noexcept
{
    return m_writeFd;
}

void WakeupChannel::wakeup()
{
    static constexpr char one = 'w';
    const int n = SocketOps::write(m_writeFd, &one, sizeof(one));
    (void)n;
}

void WakeupChannel::handleRead()
{
    char buf[256];
    for (;;)
    {
        const int n = SocketOps::read(m_readFd, buf, sizeof(buf));
        if (n <= 0)
        {
            break;
        }

        if (n < static_cast<int>(sizeof(buf)))
        {
            break;
        }
    }
}

}  // namespace dbase::net