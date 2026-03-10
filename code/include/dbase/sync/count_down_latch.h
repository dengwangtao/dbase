#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace dbase::sync
{
class CountDownLatch
{
    public:
        explicit CountDownLatch(std::int32_t count);

        CountDownLatch(const CountDownLatch&) = delete;
        CountDownLatch& operator=(const CountDownLatch&) = delete;

        void wait();
        [[nodiscard]] bool waitFor(std::uint64_t timeoutMs);
        void countDown();
        [[nodiscard]] std::int32_t count() const noexcept;

    private:
        mutable std::mutex m_mutex;
        std::condition_variable m_cv;
        std::int32_t m_count{0};
};
}  // namespace dbase::sync