#ifndef ANALYZER4071_H
#define ANALYZER4071_H

#include "scpi/scpitypes.h"

class InstrumentSession;

struct Analyzer4071SpectrumConfigResult
{
    bool ok = false;
    QString opcResponse;
    QString errorResponse;
};

struct Analyzer4071PeakResult
{
    bool ok = false;
    double peakFrequencyMHz = 0.0;
    double peakPowerDbm = 0.0;
};

class Analyzer4071
{
public:
    explicit Analyzer4071(InstrumentSession *session);

    InstrumentSession *session() const;
    bool isConnected() const;
    ScpiReply identify(int timeoutMs = 1500);
    ScpiReply readError(int timeoutMs = 1200);
    ScpiReply measureBasicVoltage(int timeoutMs = 2000);
    Analyzer4071SpectrumConfigResult configureSpectrum(double centerMHz,
                                                       double spanMHz,
                                                       double rbwKHz,
                                                       double vbwKHz);
    Analyzer4071PeakResult readPeak();
    bool stopMeasurement();
    bool stopMeasurement(const ScpiRequestOptions &options);

private:
    double parseFirstDouble(const QString &text, bool *ok) const;

    InstrumentSession *m_session;
};

#endif
