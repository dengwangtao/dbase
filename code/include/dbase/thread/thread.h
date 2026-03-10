#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <stop_token>
#include <string>
#include <thread>

namespace dbase::thread
{
class Thread
{
    public:
        using StopFunction = std::function<void(std::stop_token)>;
        using Function = std::function<void()>;

        Thread() = default;
        explicit Thread(StopFunction func, std::string name = {});
        explicit Thread(Function func, std::string name = {});

        Thread(const Thread&) = delete;
        Thread& operator=(const Thread&) = delete;

        Thread(Thread&& other) noexcept;
        Thread& operator=(Thread&& other) noexcept;

        ~Thread();

        void start();
        void join();
        void detach();
        void requestStop();

        [[nodiscard]] bool joinable() const noexcept;
        [[nodiscard]] bool started() const noexcept;
        [[nodiscard]] bool stopRequested() const noexcept;

        [[nodiscard]] std::uint64_t tid() const noexcept;
        [[nodiscard]] const std::string& name() const noexcept;

        void setName(std::string name);

    private:
        StopFunction m_func;
        std::string m_name{"thread"};
        std::jthread m_thread;
        std::atomic<bool> m_started{false};
        std::atomic<bool> m_stopRequested{false};
        std::atomic<std::uint64_t> m_tid{0};
};
}  // namespace dbase::thread