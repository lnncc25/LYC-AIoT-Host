#ifndef CASE81CANCELLATIONTOKEN_H
#define CASE81CANCELLATIONTOKEN_H

#include <atomic>

class Case81CancellationToken
{
public:
    void requestCancellation()
    {
        m_cancelled.store(true, std::memory_order_release);
    }

    bool isCancellationRequested() const
    {
        return m_cancelled.load(std::memory_order_acquire);
    }

private:
    std::atomic_bool m_cancelled{false};
};

#endif
