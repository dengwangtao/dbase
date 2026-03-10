#include "dbase/thread/current_thread.h"

#include "dbase/platform/process.h"

#include <chrono>
#include <string>
#include <thread>

#if defined(_WIN32)
#include <Windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include <pthread.h>
#endif

namespace dbase::thread
{
namespace current_thread
{
namespace
{
thread_local std::string t_name = "unknown";

std::string trimThreadName(std::string_view name)
{
    constexpr std::size_t kMaxLen =
#if defined(__linux__) || defined(__APPLE__)
            15;
#else
            64;
#endif

    if (name.empty())
    {
        return "unknown";
    }

    if (name.size() <= kMaxLen)
    {
        return std::string(name);
    }

    return std::string(name.substr(0, kMaxLen));
}
}  // namespace

std::uint64_t tid() noexcept
{
    return dbase::platform::tid();
}

const std::string& name() noexcept
{
    return t_name;
}

void setName(std::string_view name)
{
    t_name = trimThreadName(name);

#if defined(_WIN32)
    const std::wstring wideName(t_name.begin(), t_name.end());
    ::SetThreadDescription(::GetCurrentThread(), wideName.c_str());
#elif defined(__linux__)
    ::pthread_setname_np(::pthread_self(), t_name.c_str());
#elif defined(__APPLE__)
    ::pthread_setname_np(t_name.c_str());
#endif
}

void sleepForMs(std::uint64_t ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

void sleepForUs(std::uint64_t us)
{
    std::this_thread::sleep_for(std::chrono::microseconds(us));
}

}  // namespace current_thread
}  // namespace dbase::thread