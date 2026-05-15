#pragma once

#include <coroutine>
#include <exception>
#include <optional>
#include <utility>

namespace dbase::coro
{

template <typename T>
class Task;

namespace detail
{
template <typename T>
struct TaskPromiseBase
{
        std::coroutine_handle<> continuation = nullptr;
        std::exception_ptr exception;

        Task<T> get_return_object();

        std::suspend_always initial_suspend() noexcept { return {}; }

        struct FinalAwaiter
        {
                bool await_ready() noexcept { return false; }
                template <typename Promise>
                std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> h) noexcept
                {
                    auto& p = h.promise();
                    return p.continuation ? p.continuation : std::noop_coroutine();
                }
                void await_resume() noexcept {}
        };

        FinalAwaiter final_suspend() noexcept { return {}; }

        void unhandled_exception() { exception = std::current_exception(); }
};

template <>
struct TaskPromiseBase<void>
{
        std::coroutine_handle<> continuation = nullptr;
        std::exception_ptr exception;

        Task<void> get_return_object();

        std::suspend_always initial_suspend() noexcept { return {}; }

        struct FinalAwaiter
        {
                bool await_ready() noexcept { return false; }
                template <typename Promise>
                std::coroutine_handle<> await_suspend(std::coroutine_handle<Promise> h) noexcept
                {
                    auto& p = h.promise();
                    return p.continuation ? p.continuation : std::noop_coroutine();
                }
                void await_resume() noexcept {}
        };

        FinalAwaiter final_suspend() noexcept { return {}; }

        void unhandled_exception() { exception = std::current_exception(); }
};

}  // namespace detail

template <typename T>
class Task
{
    public:
        struct promise_type : detail::TaskPromiseBase<T>
        {
                std::optional<T> value;

                template <typename U>
                void return_value(U&& v)
                {
                    value.emplace(std::forward<U>(v));
                }
        };

        using handle_type = std::coroutine_handle<promise_type>;

        Task() = default;
        explicit Task(handle_type h)
            : h_(h) {}

        Task(Task&& rhs) noexcept
            : h_(rhs.h_) { rhs.h_ = nullptr; }
        Task& operator=(Task&& rhs) noexcept
        {
            if (this != &rhs)
            {
                if (h_)
                    h_.destroy();
                h_ = rhs.h_;
                rhs.h_ = nullptr;
            }
            return *this;
        }

        Task(const Task&) = delete;
        Task& operator=(const Task&) = delete;

        ~Task()
        {
            if (h_)
                h_.destroy();
        }

        handle_type release() noexcept
        {
            auto tmp = h_;
            h_ = nullptr;
            return tmp;
        }

        bool valid() const noexcept { return h_ != nullptr; }
        bool done() const noexcept { return !h_ || h_.done(); }

        struct Awaiter
        {
                handle_type h;

                bool await_ready() noexcept { return !h || h.done(); }

                std::coroutine_handle<> await_suspend(std::coroutine_handle<> cont) noexcept
                {
                    h.promise().continuation = cont;
                    return h;
                }

                T await_resume()
                {
                    auto& p = h.promise();
                    if (p.exception)
                        std::rethrow_exception(p.exception);
                    return std::move(*p.value);
                }
        };

        Awaiter operator co_await() noexcept { return Awaiter{h_}; }

    private:
        handle_type h_ = nullptr;
};

template <>
class Task<void>
{
    public:
        struct promise_type : detail::TaskPromiseBase<void>
        {
                void return_void() {}
        };

        using handle_type = std::coroutine_handle<promise_type>;

        Task() = default;
        explicit Task(handle_type h)
            : h_(h) {}

        Task(Task&& rhs) noexcept
            : h_(rhs.h_) { rhs.h_ = nullptr; }
        Task& operator=(Task&& rhs) noexcept
        {
            if (this != &rhs)
            {
                if (h_)
                    h_.destroy();
                h_ = rhs.h_;
                rhs.h_ = nullptr;
            }
            return *this;
        }

        Task(const Task&) = delete;
        Task& operator=(const Task&) = delete;

        ~Task()
        {
            if (h_)
                h_.destroy();
        }

        handle_type release() noexcept
        {
            auto tmp = h_;
            h_ = nullptr;
            return tmp;
        }

        bool valid() const noexcept { return h_ != nullptr; }
        bool done() const noexcept { return !h_ || h_.done(); }

        struct Awaiter
        {
                handle_type h;

                bool await_ready() noexcept { return !h || h.done(); }

                std::coroutine_handle<> await_suspend(std::coroutine_handle<> cont) noexcept
                {
                    h.promise().continuation = cont;
                    return h;
                }

                void await_resume()
                {
                    auto& p = h.promise();
                    if (p.exception)
                        std::rethrow_exception(p.exception);
                }
        };

        Awaiter operator co_await() noexcept { return Awaiter{h_}; }

    private:
        handle_type h_ = nullptr;
};

namespace detail
{
template <typename T>
inline Task<T> TaskPromiseBase<T>::get_return_object()
{
    using promise_t = typename Task<T>::promise_type;
    return Task<T>{std::coroutine_handle<promise_t>::from_promise(
            *static_cast<promise_t*>(this))};
}

inline Task<void> TaskPromiseBase<void>::get_return_object()
{
    using promise_t = Task<void>::promise_type;
    return Task<void>{std::coroutine_handle<promise_t>::from_promise(
            *static_cast<promise_t*>(this))};
}
}  // namespace detail

}  // namespace dbase::coro
