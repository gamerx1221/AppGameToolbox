#pragma once

#include <cstdint>
#include <mutex>
#include <utility>
#include <vector>

namespace appgametoolbox {

struct FrameSyncToken {
    std::uint64_t value = 0;

    explicit operator bool() const { return value != 0; }
};

class IFrameSyncManager {
public:
    virtual ~IFrameSyncManager() = default;

    virtual FrameSyncToken SignalFrame() = 0;
    virtual bool IsComplete(FrameSyncToken token) = 0;
    virtual void Wait(FrameSyncToken token) = 0;
    virtual void WaitIdle() = 0;
};

// Traits create, signal, query, wait for, and destroy the native fence type.
// Queue submissions are ordered, so completed tokens form a contiguous range.
template <typename NativeFence, typename Traits>
class FenceSyncManager final : public IFrameSyncManager {
public:
    explicit FenceSyncManager(Traits traits) : m_traits(std::move(traits)) {}

    ~FenceSyncManager() override {
        try {
            WaitIdle();
        } catch (...) {
            // Destruction cannot report a backend synchronization failure.
        }
    }

    FrameSyncToken SignalFrame() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        NativeFence fence = m_traits.CreateFence();
        m_traits.Signal(fence);
        const FrameSyncToken token{m_nextToken++};
        m_pending.push_back({token, std::move(fence)});
        return token;
    }

    bool IsComplete(FrameSyncToken token) override {
        if (!token)
            return false;

        std::lock_guard<std::mutex> lock(m_mutex);
        CollectCompleted();
        return token.value <= m_completedToken;
    }

    void Wait(FrameSyncToken token) override {
        if (!token)
            return;

        std::lock_guard<std::mutex> lock(m_mutex);
        if (token.value <= m_completedToken)
            return;

        for (const PendingFence& pending : m_pending) {
            if (pending.token.value == token.value) {
                m_traits.Wait(pending.fence);
                CollectCompleted();
                return;
            }
        }
    }

    void WaitIdle() override {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_traits.WaitIdle();
        for (PendingFence& pending : m_pending)
            m_traits.DestroyFence(pending.fence);
        if (!m_pending.empty())
            m_completedToken = m_pending.back().token.value;
        m_pending.clear();
    }

private:
    struct PendingFence {
        FrameSyncToken token;
        NativeFence fence;
    };

    void CollectCompleted() {
        while (!m_pending.empty() && m_traits.IsComplete(m_pending.front().fence)) {
            m_completedToken = m_pending.front().token.value;
            m_traits.DestroyFence(m_pending.front().fence);
            m_pending.erase(m_pending.begin());
        }
    }

    Traits m_traits;
    std::mutex m_mutex;
    std::vector<PendingFence> m_pending;
    std::uint64_t m_nextToken = 1;
    std::uint64_t m_completedToken = 0;
};

} // namespace appgametoolbox
