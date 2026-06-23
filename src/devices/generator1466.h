#ifndef GENERATOR1466_H
#define GENERATOR1466_H

#include "scpi/scpitypes.h"

class InstrumentSession;

struct Generator1466CwConfigResult
{
    bool ok = false;
    bool clamped = false;
    double requestedPowerDbm = 0.0;
    double outputPowerDbm = 0.0;
    QString frequencyResponse;
    QString powerResponse;
    QString outputResponse;
    QString errorResponse;
};

class Generator1466
{
public:
    explicit Generator1466(InstrumentSession *session);

    InstrumentSession *session() const;
    bool isConnected() const;
    ScpiReply identify(int timeoutMs = 1500);
    ScpiReply readError(int timeoutMs = 1200);
    Generator1466CwConfigResult configureCw(double freqMHz,
                                            double powerDbm,
                                            bool outputOn,
                                            bool enforceSafeClamp = true);
    bool shutdownOutput();
    bool shutdownOutput(const ScpiRequestOptions &options);

private:
    InstrumentSession *m_session;
};

#endif
