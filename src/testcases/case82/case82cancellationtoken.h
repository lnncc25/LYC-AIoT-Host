#ifndef CASE82CANCELLATIONTOKEN_H
#define CASE82CANCELLATIONTOKEN_H

#include "scpitypes.h"

#include <atomic>

class Case82CancellationToken : public IScpiCancellation
{
public:
    void requestCancellation()
    {
        m_cancelled.store(true);
    }

    bool isCancellationRequested() const override
    {
        return m_cancelled.load();
    }

private:
    std::atomic_bool m_cancelled{false};
};

#endif
