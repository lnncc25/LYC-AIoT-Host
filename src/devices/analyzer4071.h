#ifndef ANALYZER4071_H
#define ANALYZER4071_H

#include "scpi/scpitypes.h"

class InstrumentSession;

class Analyzer4071
{
public:
    explicit Analyzer4071(InstrumentSession *session);

    InstrumentSession *session() const;
    ScpiReply identify(int timeoutMs = 1500);
    ScpiReply readError(int timeoutMs = 1200);

private:
    InstrumentSession *m_session;
};

#endif
