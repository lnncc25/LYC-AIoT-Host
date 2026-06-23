#ifndef CASE85CANCELLATIONTOKEN_H
#define CASE85CANCELLATIONTOKEN_H

#include "scpitypes.h"

#include <atomic>

class Case85CancellationToken : public IScpiCancellation
{
public:
    Case85CancellationToken()
        : m_requested(false)
    {
    }

    void requestCancellation()
    {
        m_requested.store(true, std::memory_order_release);
    }

    bool isCancellationRequested() const override
    {
        return m_requested.load(std::memory_order_acquire);
    }

private:
    std::atomic_bool m_requested;
};

#endif
