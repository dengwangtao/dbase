#include "dbase/coro/coroutine_scheduler.h"

namespace dbase::coro
{

thread_local bool CoroutineScheduler::tls_in_coro_ = false;
thread_local CoroutineScheduler* CoroutineScheduler::tls_sched_ = nullptr;
thread_local uint64_t CoroutineScheduler::tls_id_ = 0;

CoroutineScheduler::CoroutineScheduler() = default;

CoroutineScheduler::~CoroutineScheduler()
{
    for (auto& kv : coros_)
    {
        if (kv.second.root)
            kv.second.root.destroy();
        kv.second.root = {};
        kv.second.current = {};
    }
    coros_.clear();
    ready_.clear();
}

int32_t CoroutineScheduler::Init()
{
    return 0;
}

uint64_t CoroutineScheduler::AllocId()
{
    return next_id_.fetch_add(1, std::memory_order_relaxed);
}

CoroutineScheduler::Entry* CoroutineScheduler::GetEntry(uint64_t id)
{
    auto it = coros_.find(id);
    if (it == coros_.end())
        return nullptr;
    return &it->second;
}

int32_t CoroutineScheduler::Create(Task<void>&& task, uint64_t& id)
{
    auto h = task.release();
    if (!h)
        return -1;

    id = AllocId();

    // root=current=根协程句柄（之后 Yield 会不断刷新 current）
    coros_.emplace(id, Entry{h, h});
    return 0;
}

uint64_t CoroutineScheduler::Spawn(Task<void>&& task)
{
    uint64_t id = 0;
    if (Create(std::move(task), id) != 0)
        return 0;
    return id;
}

void CoroutineScheduler::UpdateCurrent(std::coroutine_handle<> h) noexcept
{
    if (!tls_sched_ || tls_id_ == 0)
        return;

    auto e = tls_sched_->GetEntry(tls_id_);
    if (!e)
        return;

    e->current = h;
}

int32_t CoroutineScheduler::Resume(uint64_t id)
{
    auto e = GetEntry(id);
    if (!e || !e->current)
        return -1;

    // RAII 标记“处于协程中”
    struct Guard
    {
            CoroutineScheduler* prev_sched = nullptr;
            uint64_t prev_id = 0;
            bool prev_in = false;

            Guard(CoroutineScheduler* s, uint64_t id)
            {
                prev_sched = tls_sched_;
                prev_id = tls_id_;
                prev_in = tls_in_coro_;

                tls_sched_ = s;
                tls_id_ = id;
                tls_in_coro_ = true;
            }

            ~Guard()
            {
                tls_sched_ = prev_sched;
                tls_id_ = prev_id;
                tls_in_coro_ = prev_in;
            }
    } g(this, id);

    auto h = e->current;
    if (!h.done())
        h.resume();

    if (e->current && e->current.done())
        e->current = {};

    return 0;
}

int32_t CoroutineScheduler::Restart(uint64_t id, Task<void>&& task)
{
    auto e = GetEntry(id);
    if (!e)
        return -1;

    if (e->root)
        e->root.destroy();

    e->root = {};
    e->current = {};

    auto h = task.release();
    if (!h)
        return -2;

    e->root = h;
    e->current = h;
    return 0;
}

int32_t CoroutineScheduler::Destroy(uint64_t id)
{
    auto it = coros_.find(id);
    if (it == coros_.end())
        return -1;

    if (it->second.root)
        it->second.root.destroy();

    it->second.root = {};
    it->second.current = {};
    coros_.erase(it);
    return 0;
}

bool CoroutineScheduler::IsInCoroutine() const
{
    return tls_in_coro_ && tls_sched_ == this;
}

void CoroutineScheduler::YieldAwaiter::await_suspend(std::coroutine_handle<> h) noexcept
{
    sched->UpdateCurrent(h);
}

void CoroutineScheduler::Schedule(uint64_t id)
{
    if (id == 0)
        return;

    auto e = GetEntry(id);
    if (!e || !e->current)
        return;

    ready_.push_back(id);
}

int32_t CoroutineScheduler::RunOnce()
{
    while (!ready_.empty())
    {
        uint64_t id = ready_.front();
        ready_.pop_front();

        auto e = GetEntry(id);
        if (!e || !e->current)
            continue;

        Resume(id);
        return 1;
    }
    return 0;
}

int32_t CoroutineScheduler::RunAll()
{
    int32_t n = 0;
    while (RunOnce() > 0)
        ++n;
    return n;
}

}  // namespace dbase::coro