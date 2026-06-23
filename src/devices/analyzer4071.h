#ifndef ANALYZER4071_H
#define ANALYZER4071_H

#include "scpi/scpitypes.h"

#include <QStringList>

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

struct Analyzer4071AclrConfigResult
{
    bool ok = false;
    QStringList errorQueue;
};

struct Analyzer4071AclrMeasurementResult
{
    bool ok = false;
    double mainPowerDbm = 0.0;
    double leftAclrDb = 0.0;
    double rightAclrDb = 0.0;
    QString response;
    QString details;
};

struct Analyzer4071FileResult
{
    bool ok = false;
    QStringList errorQueue;
};

struct Analyzer4071BinaryFileResult
{
    bool ok = false;
    QByteArray payload;
    QString error;
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
    Analyzer4071AclrConfigResult configureAclr(double centerFreqMHz,
                                               double carrierBWKHz,
                                               double integrationBWKHz,
                                               double offsetKHz);
    Analyzer4071AclrMeasurementResult measureAclr();
    Analyzer4071PeakResult readPeak();
    Analyzer4071FileResult saveScreenSnapshot(const QString &fileName);
    Analyzer4071BinaryFileResult downloadFile(const QString &remoteFileName,
                                              int timeoutMs = 15000);
    Analyzer4071FileResult deleteFile(const QString &remoteFileName);
    bool stopMeasurement();
    bool stopMeasurement(const ScpiRequestOptions &options);

private:
    double parseFirstDouble(const QString &text, bool *ok) const;
    bool parseAcpowerResponse(const QString &response,
                              double &mainPowerDbm,
                              double &leftAclrDb,
                              double &rightAclrDb,
                              QString *details) const;
    QStringList drainErrorQueue(int maxReads = 8);

    InstrumentSession *m_session;
};

#endif
