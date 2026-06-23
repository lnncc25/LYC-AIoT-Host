#ifndef GENERATOR1466_H
#define GENERATOR1466_H

#include "scpi/scpitypes.h"

#include <QStringList>

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

struct Generator1466ArbConfigResult
{
    bool ok = false;
    QString sampleClockResponse;
    QStringList errorQueue;
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
    Generator1466ArbConfigResult configureArbPlayback(const QString &fileName,
                                                      double freqMHz,
                                                      double powerDbm,
                                                      double sampleClockMHz);
    bool shutdownOutput();
    bool shutdownOutput(const ScpiRequestOptions &options);

private:
    QStringList drainErrorQueue(int maxReads = 8);

    InstrumentSession *m_session;
};

#endif
