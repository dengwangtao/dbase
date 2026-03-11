#pragma once

#include "dbase/net/event_loop_thread.h"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace dbase::net
{
class EventLoopThreadPool
{
    public:
        EventLoopThreadPool(
                EventLoop* baseLoop,
                std::string poolName,
                std::size_t threadCount);

        ~EventLoopThreadPool();

        EventLoopThreadPool(const EventLoopThreadPool&) = delete;
        EventLoopThreadPool& operator=(const EventLoopThreadPool&) = delete;

        EventLoopThreadPool(EventLoopThreadPool&&) = delete;
        EventLoopThreadPool& operator=(EventLoopThreadPool&&) = delete;

        void start();

        [[nodiscard]] bool started() const noexcept;
        [[nodiscard]] EventLoop* baseLoop() const noexcept;
        [[nodiscard]] const std::string& poolName() const noexcept;
        [[nodiscard]] std::size_t threadCount() const noexcept;
        [[nodiscard]] std::size_t loopCount() const noexcept;

        [[nodiscard]] EventLoop* getNextLoop();
        [[nodiscard]] EventLoop* getLoopForHash(std::size_t hashCode);

        [[nodiscard]] const std::vector<EventLoop*>& loops() const noexcept;

    private:
        EventLoop* m_baseLoop{nullptr};
        std::string m_poolName;
        std::size_t m_threadCount{0};
        bool m_started{false};
        std::size_t m_next{0};

        std::vector<std::unique_ptr<EventLoopThread>> m_threads;
        std::vector<EventLoop*> m_loops;
};
}  // namespace dbase::net