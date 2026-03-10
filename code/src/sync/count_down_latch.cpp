#include "dbase/sync/count_down_latch.h"

#include <chrono>
#include <stdexcept>

namespace dbase::sync
{
CountDownLatch::CountDownLatch(std::int32_t count)
    : m_count(count)
{
    if (count < 0)
    {
        throw std::invalid_argument("CountDownLatch count must be non-negative");
    }
}

void CountDownLatch::wait()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this]()
              { return m_count == 0; });
}

bool CountDownLatch::waitFor(std::uint64_t timeoutMs)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_cv.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]()
                         { return m_count == 0; });
}

void CountDownLatch::countDown()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_count == 0)
    {
        return;
    }

    --m_count;
    if (m_count == 0)
    {
        m_cv.notify_all();
    }
}

std::int32_t CountDownLatch::count() const noexcept
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_count;
}

}  // namespace dbase::sync