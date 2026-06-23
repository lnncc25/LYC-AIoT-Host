#ifndef GENERATOR1466_H
#define GENERATOR1466_H

#include "scpi/scpitypes.h"

class InstrumentSession;

class Generator1466
{
public:
    explicit Generator1466(InstrumentSession *session);

    InstrumentSession *session() const;
    ScpiReply identify(int timeoutMs = 1500);
    ScpiReply readError(int timeoutMs = 1200);

private:
    InstrumentSession *m_session;
};

#endif
