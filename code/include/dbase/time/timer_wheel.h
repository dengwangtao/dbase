#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace dbase::time
{

class DoubleLink;

struct DoubleLinkNode
{
        DoubleLink* pOwner;
        DoubleLinkNode* pNext;
        DoubleLinkNode* pPrev;

        DoubleLinkNode() noexcept
            : pOwner(nullptr), pNext(nullptr), pPrev(nullptr)
        {
        }

        virtual ~DoubleLinkNode();

        DoubleLinkNode(const DoubleLinkNode&) noexcept
            : pOwner(nullptr), pNext(nullptr), pPrev(nullptr)
        {
        }

        DoubleLinkNode& operator=(const DoubleLinkNode&) noexcept
        {
            pOwner = nullptr;
            pNext = nullptr;
            pPrev = nullptr;
            return *this;
        }
};

class DoubleLink
{
    public:
        DoubleLink() noexcept
            : count_(0)
        {
            head_.pNext = &tail_;
            tail_.pPrev = &head_;
        }

        ~DoubleLink()
        {
            Clear();
        }

        DoubleLink(const DoubleLink&) = delete;
        DoubleLink& operator=(const DoubleLink&) = delete;

        [[nodiscard]] uint32_t Size() const noexcept
        {
            return count_;
        }

        [[nodiscard]] bool Empty() const noexcept
        {
            return head_.pNext == &tail_;
        }

        void Clear() noexcept
        {
            while (Pop() != nullptr)
            {
            }
        }

        [[nodiscard]] DoubleLinkNode* GetFirst() noexcept
        {
            return Empty() ? nullptr : head_.pNext;
        }

        [[nodiscard]] const DoubleLinkNode* GetFirst() const noexcept
        {
            return Empty() ? nullptr : head_.pNext;
        }

        [[nodiscard]] DoubleLinkNode* GetHead() noexcept
        {
            return &head_;
        }

        [[nodiscard]] DoubleLinkNode* GetTail() noexcept
        {
            return &tail_;
        }

        void Push(DoubleLinkNode* node) noexcept
        {
            if (node == nullptr)
            {
                return;
            }

            if (!isDetached(node))
            {
                return;
            }

            tail_.pPrev->pNext = node;
            node->pPrev = tail_.pPrev;
            node->pNext = &tail_;
            node->pOwner = this;
            tail_.pPrev = node;
            ++count_;
        }

        void PushFront(DoubleLinkNode* node) noexcept
        {
            if (node == nullptr)
            {
                return;
            }

            if (!isDetached(node))
            {
                return;
            }

            node->pPrev = &head_;
            node->pNext = head_.pNext;
            head_.pNext->pPrev = node;
            head_.pNext = node;
            node->pOwner = this;
            ++count_;
        }

        DoubleLinkNode* Pop() noexcept
        {
            if (Empty())
            {
                return nullptr;
            }

            DoubleLinkNode* ret = head_.pNext;
            Remove(ret);
            return ret;
        }

        static void Remove(DoubleLinkNode* node) noexcept
        {
            if (node == nullptr)
            {
                return;
            }

            if (!isLinked(node))
            {
                return;
            }

            node->pPrev->pNext = node->pNext;
            node->pNext->pPrev = node->pPrev;
            --node->pOwner->count_;
            node->pOwner = nullptr;
            node->pNext = nullptr;
            node->pPrev = nullptr;
        }

    private:
        static bool isDetached(const DoubleLinkNode* node) noexcept
        {
            return node->pOwner == nullptr && node->pNext == nullptr && node->pPrev == nullptr;
        }

        static bool isLinked(const DoubleLinkNode* node) noexcept
        {
            return node->pOwner != nullptr && node->pNext != nullptr && node->pPrev != nullptr;
        }

    private:
        DoubleLinkNode head_;
        DoubleLinkNode tail_;
        uint32_t count_;
};

inline DoubleLinkNode::~DoubleLinkNode()
{
    if (pOwner != nullptr)
    {
        DoubleLink::Remove(this);
    }
}

class Timer : public DoubleLinkNode
{
    public:
        Timer() noexcept
            : timeout_(0)
        {
        }

        ~Timer() override = default;

        virtual void onTimeOut() = 0;

        void setTimeOut(uint32_t timeout) noexcept
        {
            timeout_ = timeout;
        }

        [[nodiscard]] uint32_t getTimeOut() const noexcept
        {
            return timeout_;
        }

        [[nodiscard]] bool isRegistered() const noexcept
        {
            return pOwner != nullptr;
        }

    private:
        uint32_t timeout_;
};

class TimerHandler : public Timer
{
    public:
        explicit TimerHandler(std::function<void()> timeout_handler = {})
            : timeout_handler_(std::move(timeout_handler))
        {
        }

        void setTimeOutHandler(std::function<void()> timeout_handler)
        {
            timeout_handler_ = std::move(timeout_handler);
        }

        void onTimeOut() override
        {
            if (timeout_handler_)
            {
                timeout_handler_();
            }
        }

    private:
        std::function<void()> timeout_handler_;
};

class TimerMgr
{
        struct Wheel
        {
                explicit Wheel(uint32_t no);

                void clear() noexcept;
                [[nodiscard]] uint32_t slotCount() const noexcept;

                uint32_t wheel_no;
                std::vector<DoubleLink> tlist;
                uint32_t cur_slot;
        };

    public:
        static constexpr uint32_t WHEEL_CNT = 5;
        static constexpr std::array<uint32_t, WHEEL_CNT> ELEMENT_CNT_PER_WHEEL = {256, 64, 64, 64, 64};
        static constexpr std::array<uint32_t, WHEEL_CNT> RIGHT_SHIFT_PER_WHEEL = {8, 6, 6, 6, 6};
        static constexpr std::array<uint32_t, WHEEL_CNT> BASE_TICK_PER_WHEEL = {
                1u,
                256u,
                256u * 64u,
                256u * 64u * 64u,
                256u * 64u * 64u * 64u};

        TimerMgr();
        ~TimerMgr() = default;

        int32_t registerTimer(Timer* timer, uint32_t timeout);
        static int32_t unRegisterTimer(Timer* timer) noexcept;

        void initTick();
        void tick();

        void initTickMs();
        void tickMs();

        void clear() noexcept;

    private:
        void step();
        void tickInternal(uint64_t cur_tick);
        [[nodiscard]] uint64_t getNow() const noexcept;
        void resetWheels() noexcept;

    private:
        std::array<Wheel, WHEEL_CNT> wheels_;
        bool is_ms_timer_;
        uint64_t last_tick_;
};

using TimerMgrPtr = std::shared_ptr<TimerMgr>;

}  // namespace dbase::time