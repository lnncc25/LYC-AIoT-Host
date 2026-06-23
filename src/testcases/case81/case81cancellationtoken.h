#ifndef CASE81CANCELLATIONTOKEN_H
#define CASE81CANCELLATIONTOKEN_H

#include "scpitypes.h"

#include <atomic>

class Case81CancellationToken : public IScpiCancellation
{
public:
    void requestCancellation()
    {
        m_cancelled.store(true, std::memory_order_release);
    }

    bool isCancellationRequested() const override
    {
        return m_cancelled.load(std::memory_order_acquire);
    }

private:
    std::atomic_bool m_cancelled{false};
};

#endif
