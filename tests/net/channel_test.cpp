#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "dbase/net/channel.h"
#include "dbase/net/event_loop.h"
#include "dbase/net/socket_ops.h"

namespace
{
struct SocketOpsInit
{
        SocketOpsInit()
        {
            dbase::net::SocketOps::initialize();
        }
};

const SocketOpsInit kSocketOpsInit{};

using dbase::net::Channel;
using dbase::net::EventLoop;
using dbase::net::kInvalidSocket;
using dbase::net::SocketOps;
using dbase::net::SocketType;

[[nodiscard]] SocketType makeValidSocket()
{
    const SocketType fd = SocketOps::createTcpNonblockingOrDie(AF_INET);
    return fd;
}

void closeSocket(SocketType fd)
{
    if (fd != kInvalidSocket)
    {
        SocketOps::close(fd);
    }
}
}  // namespace

TEST_CASE("Channel constructor rejects null loop", "[net][channel]")
{
    const SocketType fd = makeValidSocket();

    REQUIRE_THROWS_AS(Channel(nullptr, fd), std::invalid_argument);

    closeSocket(fd);
}

TEST_CASE("Channel constructor rejects invalid fd", "[net][channel]")
{
    EventLoop loop;

    REQUIRE_THROWS_AS(Channel(&loop, kInvalidSocket), std::invalid_argument);
}

TEST_CASE("Channel default state after construction", "[net][channel]")
{
    EventLoop loop;
    const SocketType fd = makeValidSocket();

    Channel channel(&loop, fd);

    REQUIRE(channel.ownerLoop() == &loop);
    REQUIRE(channel.fd() == fd);
    REQUIRE(channel.events() == Channel::kNoneEvent);
    REQUIRE(channel.revents() == Channel::kNoneEvent);
    REQUIRE(channel.isNoneEvent());
    REQUIRE_FALSE(channel.isReading());
    REQUIRE_FALSE(channel.isWriting());
    REQUIRE_FALSE(channel.edgeTriggered());

    closeSocket(fd);
}

TEST_CASE("Channel setEdgeTriggered toggles edge state", "[net][channel]")
{
    EventLoop loop;
    const SocketType fd = makeValidSocket();

    Channel channel(&loop, fd);

    REQUIRE_FALSE(channel.edgeTriggered());

    channel.setEdgeTriggered(true);
    REQUIRE(channel.edgeTriggered());

    channel.setEdgeTriggered(false);
    REQUIRE_FALSE(channel.edgeTriggered());

    closeSocket(fd);
}

TEST_CASE("Channel setRevents stores returned events", "[net][channel]")
{
    EventLoop loop;
    const SocketType fd = makeValidSocket();

    Channel channel(&loop, fd);

    const std::uint32_t flags =
            Channel::kReadEvent | Channel::kWriteEvent | Channel::kErrorEvent | Channel::kCloseEvent;

    channel.setRevents(flags);

    REQUIRE(channel.revents() == flags);

    closeSocket(fd);
}

TEST_CASE("Channel enableReading sets read interest", "[net][channel]")
{
    EventLoop loop;
    const SocketType fd = makeValidSocket();

    Channel channel(&loop, fd);

    channel.enableReading();

    REQUIRE(channel.isReading());
    REQUIRE_FALSE(channel.isWriting());
    REQUIRE_FALSE(channel.isNoneEvent());
    REQUIRE((channel.events() & Channel::kReadEvent) != 0);

    closeSocket(fd);
}

TEST_CASE("Channel enableWriting sets write interest", "[net][channel]")
{
    EventLoop loop;
    const SocketType fd = makeValidSocket();

    Channel channel(&loop, fd);

    channel.enableWriting();

    REQUIRE_FALSE(channel.isReading());
    REQUIRE(channel.isWriting());
    REQUIRE_FALSE(channel.isNoneEvent());
    REQUIRE((channel.events() & Channel::kWriteEvent) != 0);

    closeSocket(fd);
}

TEST_CASE("Channel enableReading and enableWriting can coexist", "[net][channel]")
{
    EventLoop loop;
    const SocketType fd = makeValidSocket();

    Channel channel(&loop, fd);

    channel.enableReading();
    channel.enableWriting();

    REQUIRE(channel.isReading());
    REQUIRE(channel.isWriting());
    REQUIRE((channel.events() & Channel::kReadEvent) != 0);
    REQUIRE((channel.events() & Channel::kWriteEvent) != 0);

    closeSocket(fd);
}

TEST_CASE("Channel disableReading clears only read interest", "[net][channel]")
{
    EventLoop loop;
    const SocketType fd = makeValidSocket();

    Channel channel(&loop, fd);
    channel.enableReading();
    channel.enableWriting();

    channel.disableReading();

    REQUIRE_FALSE(channel.isReading());
    REQUIRE(channel.isWriting());
    REQUIRE((channel.events() & Channel::kReadEvent) == 0);
    REQUIRE((channel.events() & Channel::kWriteEvent) != 0);

    closeSocket(fd);
}

TEST_CASE("Channel disableWriting clears only write interest", "[net][channel]")
{
    EventLoop loop;
    const SocketType fd = makeValidSocket();

    Channel channel(&loop, fd);
    channel.enableReading();
    channel.enableWriting();

    channel.disableWriting();

    REQUIRE(channel.isReading());
    REQUIRE_FALSE(channel.isWriting());
    REQUIRE((channel.events() & Channel::kReadEvent) != 0);
    REQUIRE((channel.events() & Channel::kWriteEvent) == 0);

    closeSocket(fd);
}

TEST_CASE("Channel disableAll clears all interests", "[net][channel]")
{
    EventLoop loop;
    const SocketType fd = makeValidSocket();

    Channel channel(&loop, fd);
    channel.enableReading();
    channel.enableWriting();

    channel.disableAll();

    REQUIRE(channel.isNoneEvent());
    REQUIRE_FALSE(channel.isReading());
    REQUIRE_FALSE(channel.isWriting());
    REQUIRE(channel.events() == Channel::kNoneEvent);

    closeSocket(fd);
}

TEST_CASE("Channel read callback fires when read event is present", "[net][channel]")
{
    EventLoop loop;
    const SocketType fd = makeValidSocket();

    Channel channel(&loop, fd);

    std::atomic<int> readCount{0};

    channel.setReadCallback([&]()
                            { readCount.fetch_add(1, std::memory_order_relaxed); });

    channel.setRevents(Channel::kReadEvent);
    channel.handleEvent();

    REQUIRE(readCount.load(std::memory_order_relaxed) == 1);

    closeSocket(fd);
}

TEST_CASE("Channel write callback fires when write event is present", "[net][channel]")
{
    EventLoop loop;
    const SocketType fd = makeValidSocket();

    Channel channel(&loop, fd);

    std::atomic<int> writeCount{0};

    channel.setWriteCallback([&]()
                             { writeCount.fetch_add(1, std::memory_order_relaxed); });

    channel.setRevents(Channel::kWriteEvent);
    channel.handleEvent();

    REQUIRE(writeCount.load(std::memory_order_relaxed) == 1);

    closeSocket(fd);
}

TEST_CASE("Channel error callback fires when error event is present", "[net][channel]")
{
    EventLoop loop;
    const SocketType fd = makeValidSocket();

    Channel channel(&loop, fd);

    std::atomic<int> errorCount{0};

    channel.setErrorCallback([&]()
                             { errorCount.fetch_add(1, std::memory_order_relaxed); });

    channel.setRevents(Channel::kErrorEvent);
    channel.handleEvent();

    REQUIRE(errorCount.load(std::memory_order_relaxed) == 1);

    closeSocket(fd);
}

TEST_CASE("Channel close callback fires when close event is present", "[net][channel]")
{
    EventLoop loop;
    const SocketType fd = makeValidSocket();

    Channel channel(&loop, fd);

    std::atomic<int> closeCount{0};

    channel.setCloseCallback([&]()
                             { closeCount.fetch_add(1, std::memory_order_relaxed); });

    channel.setRevents(Channel::kCloseEvent);
    channel.handleEvent();

    REQUIRE(closeCount.load(std::memory_order_relaxed) == 1);

    closeSocket(fd);
}

TEST_CASE("Channel handleEvent does nothing when callbacks are unset", "[net][channel]")
{
    EventLoop loop;
    const SocketType fd = makeValidSocket();

    Channel channel(&loop, fd);

    channel.setRevents(Channel::kReadEvent | Channel::kWriteEvent | Channel::kErrorEvent | Channel::kCloseEvent);

    REQUIRE_NOTHROW(channel.handleEvent());

    closeSocket(fd);
}

TEST_CASE("Channel handleEvent dispatches callbacks in error read write close order", "[net][channel]")
{
    EventLoop loop;
    const SocketType fd = makeValidSocket();

    Channel channel(&loop, fd);
    std::vector<std::string> calls;

    channel.setErrorCallback([&]()
                             { calls.emplace_back("error"); });
    channel.setReadCallback([&]()
                            { calls.emplace_back("read"); });
    channel.setWriteCallback([&]()
                             { calls.emplace_back("write"); });
    channel.setCloseCallback([&]()
                             { calls.emplace_back("close"); });

    channel.setRevents(Channel::kCloseEvent | Channel::kWriteEvent | Channel::kReadEvent | Channel::kErrorEvent);

    channel.handleEvent();

    REQUIRE(calls.size() == 4);
    REQUIRE(calls[0] == "error");
    REQUIRE(calls[1] == "read");
    REQUIRE(calls[2] == "write");
    REQUIRE(calls[3] == "close");

    closeSocket(fd);
}

TEST_CASE("Channel handleEvent can dispatch multiple selected callbacks", "[net][channel]")
{
    EventLoop loop;
    const SocketType fd = makeValidSocket();

    Channel channel(&loop, fd);

    std::atomic<int> readCount{0};
    std::atomic<int> writeCount{0};

    channel.setReadCallback([&]()
                            { readCount.fetch_add(1, std::memory_order_relaxed); });
    channel.setWriteCallback([&]()
                             { writeCount.fetch_add(1, std::memory_order_relaxed); });

    channel.setRevents(Channel::kReadEvent | Channel::kWriteEvent);
    channel.handleEvent();

    REQUIRE(readCount.load(std::memory_order_relaxed) == 1);
    REQUIRE(writeCount.load(std::memory_order_relaxed) == 1);

    closeSocket(fd);
}

TEST_CASE("Channel tie prevents callbacks after tied object expires", "[net][channel]")
{
    EventLoop loop;
    const SocketType fd = makeValidSocket();

    Channel channel(&loop, fd);

    auto owner = std::make_shared<int>(42);
    channel.tie(owner);

    std::atomic<int> readCount{0};
    std::atomic<int> writeCount{0};
    std::atomic<int> errorCount{0};
    std::atomic<int> closeCount{0};

    channel.setReadCallback([&]()
                            { readCount.fetch_add(1, std::memory_order_relaxed); });
    channel.setWriteCallback([&]()
                             { writeCount.fetch_add(1, std::memory_order_relaxed); });
    channel.setErrorCallback([&]()
                             { errorCount.fetch_add(1, std::memory_order_relaxed); });
    channel.setCloseCallback([&]()
                             { closeCount.fetch_add(1, std::memory_order_relaxed); });

    owner.reset();

    channel.setRevents(Channel::kReadEvent | Channel::kWriteEvent | Channel::kErrorEvent | Channel::kCloseEvent);
    channel.handleEvent();

    REQUIRE(readCount.load(std::memory_order_relaxed) == 0);
    REQUIRE(writeCount.load(std::memory_order_relaxed) == 0);
    REQUIRE(errorCount.load(std::memory_order_relaxed) == 0);
    REQUIRE(closeCount.load(std::memory_order_relaxed) == 0);

    closeSocket(fd);
}

TEST_CASE("Channel tie allows callbacks while tied object is alive", "[net][channel]")
{
    EventLoop loop;
    const SocketType fd = makeValidSocket();

    Channel channel(&loop, fd);

    auto owner = std::make_shared<int>(7);
    channel.tie(owner);

    std::atomic<int> readCount{0};

    channel.setReadCallback([&]()
                            { readCount.fetch_add(1, std::memory_order_relaxed); });

    channel.setRevents(Channel::kReadEvent);
    channel.handleEvent();

    REQUIRE(readCount.load(std::memory_order_relaxed) == 1);

    closeSocket(fd);
}

TEST_CASE("Channel set callbacks can replace previous callbacks", "[net][channel]")
{
    EventLoop loop;
    const SocketType fd = makeValidSocket();

    Channel channel(&loop, fd);

    std::atomic<int> first{0};
    std::atomic<int> second{0};

    channel.setReadCallback([&]()
                            { first.fetch_add(1, std::memory_order_relaxed); });
    channel.setReadCallback([&]()
                            { second.fetch_add(1, std::memory_order_relaxed); });

    channel.setRevents(Channel::kReadEvent);
    channel.handleEvent();

    REQUIRE(first.load(std::memory_order_relaxed) == 0);
    REQUIRE(second.load(std::memory_order_relaxed) == 1);

    closeSocket(fd);
}

TEST_CASE("Channel event state remains unchanged after handleEvent", "[net][channel]")
{
    EventLoop loop;
    const SocketType fd = makeValidSocket();

    Channel channel(&loop, fd);

    channel.enableReading();
    channel.enableWriting();
    channel.setRevents(Channel::kReadEvent | Channel::kWriteEvent);

    std::atomic<int> readCount{0};
    std::atomic<int> writeCount{0};

    channel.setReadCallback([&]()
                            { readCount.fetch_add(1, std::memory_order_relaxed); });
    channel.setWriteCallback([&]()
                             { writeCount.fetch_add(1, std::memory_order_relaxed); });

    channel.handleEvent();

    REQUIRE(channel.isReading());
    REQUIRE(channel.isWriting());
    REQUIRE(channel.events() == (Channel::kReadEvent | Channel::kWriteEvent));
    REQUIRE(channel.revents() == (Channel::kReadEvent | Channel::kWriteEvent));
    REQUIRE(readCount.load(std::memory_order_relaxed) == 1);
    REQUIRE(writeCount.load(std::memory_order_relaxed) == 1);

    closeSocket(fd);
}