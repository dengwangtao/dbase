#pragma once

#include "dbase/coro/task.h"

#include <atomic>
#include <coroutine>
#include <deque>
#include <unordered_map>

namespace dbase::coro
{

class CoroutineScheduler
{
    public:
        CoroutineScheduler();
        ~CoroutineScheduler();

        int32_t Init();

        // 创建一个协程任务（frame），返回一个全局唯一 id
        int32_t Create(Task<void>&& task, uint64_t& id);

        uint64_t Spawn(Task<void>&& task);

        // 运行/继续运行该 id 对应协程（直到下一次 co_await yield 或结束）
        int32_t Resume(uint64_t id);

        // 重启：销毁旧 frame，用新 task 替换（id 不变）
        int32_t Restart(uint64_t id, Task<void>&& task);

        // 销毁该 id 对应协程 frame
        int32_t Destroy(uint64_t id);

        bool IsInCoroutine() const;

        // 让当前协程让出执行权，回到“主循环语义”
        struct YieldAwaiter
        {
                CoroutineScheduler* sched = nullptr;

                bool await_ready() noexcept { return false; }

                // 关键：h 就是“真正挂起点”的 handle，必须记录下来
                void await_suspend(std::coroutine_handle<> h) noexcept;

                void await_resume() noexcept {}
        };

        YieldAwaiter Yield() { return YieldAwaiter{this}; }

        void Schedule(uint64_t id);

        int32_t RunOnce();

        int32_t RunAll();

    private:
        struct Entry
        {
                // root：创建时的根 handle，用来 destroy（级联释放整棵 coroutine 栈）
                std::coroutine_handle<> root;

                // current：当前真正挂起的 handle，用来 resume
                std::coroutine_handle<> current;
        };

        uint64_t AllocId();

        Entry* GetEntry(uint64_t id);

        void UpdateCurrent(std::coroutine_handle<> h) noexcept;

    private:
        std::unordered_map<uint64_t, Entry> coros_;
        std::atomic<uint64_t> next_id_{1};

        std::deque<uint64_t> ready_;

        static thread_local bool tls_in_coro_;
        static thread_local CoroutineScheduler* tls_sched_;
        static thread_local uint64_t tls_id_;
};

}  // namespace dbase::coro