#pragma once

#include "dbase/net/event_loop.h"
#include "dbase/sync/count_down_latch.h"
#include "dbase/thread/thread.h"

#include <memory>
#include <string>

namespace dbase::net
{
class EventLoopThread
{
    public:
        explicit EventLoopThread(std::string threadName = "event-loop");
        ~EventLoopThread();

        EventLoopThread(const EventLoopThread&) = delete;
        EventLoopThread& operator=(const EventLoopThread&) = delete;

        EventLoopThread(EventLoopThread&&) = delete;
        EventLoopThread& operator=(EventLoopThread&&) = delete;

        [[nodiscard]] EventLoop* startLoop();
        void stop();

        [[nodiscard]] bool started() const noexcept;
        [[nodiscard]] const std::string& threadName() const noexcept;

    private:
        void threadFunc(std::stop_token stopToken);

    private:
        std::string m_threadName;
        dbase::thread::Thread m_thread;
        dbase::sync::CountDownLatch m_latch{1};
        EventLoop* m_loop{nullptr};
        bool m_started{false};
};
}  // namespace dbase::net