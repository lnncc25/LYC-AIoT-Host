#include "analyzer4071.h"

#include "scpi/instrumentsession.h"

#include <QThread>

namespace {

bool scpiErrorIsClear(const QString &response)
{
    return response.isEmpty()
            || response.startsWith(QStringLiteral("+0"))
            || response.contains(QStringLiteral("No error"), Qt::CaseInsensitive);
}

}

Analyzer4071::Analyzer4071(InstrumentSession *session)
    : m_session(session)
{
}

InstrumentSession *Analyzer4071::session() const
{
    return m_session;
}

bool Analyzer4071::isConnected() const
{
    return m_session && m_session->isConnected();
}

ScpiReply Analyzer4071::identify(int timeoutMs)
{
    return m_session->query(QStringLiteral("*IDN?"), timeoutMs);
}

ScpiReply Analyzer4071::readError(int timeoutMs)
{
    return m_session->query(QStringLiteral(":SYSTem:ERRor?"), timeoutMs);
}

ScpiReply Analyzer4071::measureBasicVoltage(int timeoutMs)
{
    return m_session->query(QStringLiteral("MEAS:VOLT:DC?"), timeoutMs);
}

Analyzer4071SpectrumConfigResult Analyzer4071::configureSpectrum(double centerMHz,
                                                                 double spanMHz,
                                                                 double rbwKHz,
                                                                 double vbwKHz)
{
    const ScpiWriteResult clearResult = m_session->send(QStringLiteral("*CLS"));
    QThread::msleep(50);
    const ScpiWriteResult configureResult =
        m_session->send(QStringLiteral(":CONFigure:SANalyzer"));
    const ScpiWriteResult centerResult =
        m_session->send(QStringLiteral(":SENSe:FREQuency:CENTer %1MHz").arg(centerMHz, 0, 'f', 6));
    const ScpiWriteResult spanResult =
        m_session->send(QStringLiteral(":SENSe:FREQuency:SPAN %1MHz").arg(spanMHz, 0, 'f', 6));
    const ScpiWriteResult rbwResult =
        m_session->send(QStringLiteral(":SENSe:BANDwidth:RESolution %1kHz").arg(rbwKHz, 0, 'f', 3));
    const ScpiWriteResult vbwResult =
        m_session->send(QStringLiteral(":SENSe:BANDwidth:VIDeo %1kHz").arg(vbwKHz, 0, 'f', 3));
    const ScpiWriteResult continuousResult =
        m_session->send(QStringLiteral(":INITiate:CONTinuous OFF"));
    const ScpiWriteResult immediateResult =
        m_session->send(QStringLiteral(":INITiate:IMMediate"));
    const ScpiReply opcReply = m_session->query(QStringLiteral("*OPC?"), 2500);

    const ScpiReply errorReply = m_session->query(QStringLiteral(":SYSTem:ERRor?"), 1200);
    Analyzer4071SpectrumConfigResult result;
    result.opcResponse = opcReply.text;
    result.errorResponse = errorReply.text;
    result.ok = clearResult.isSuccess()
            && configureResult.isSuccess()
            && centerResult.isSuccess()
            && spanResult.isSuccess()
            && rbwResult.isSuccess()
            && vbwResult.isSuccess()
            && continuousResult.isSuccess()
            && immediateResult.isSuccess()
            && opcReply.status == ScpiStatus::Success
            && opcReply.text.trimmed() == QStringLiteral("1")
            && errorReply.status == ScpiStatus::Success
            && scpiErrorIsClear(errorReply.text);
    return result;
}

Analyzer4071PeakResult Analyzer4071::readPeak()
{
    m_session->send(QStringLiteral(":CALCulate:MARKer1:MAXimum"));
    const ScpiReply freqReply = m_session->query(QStringLiteral(":CALCulate:MARKer1:X?"), 1600);
    const ScpiReply powerReply = m_session->query(QStringLiteral(":CALCulate:MARKer1:Y?"), 1600);

    bool freqOk = false;
    bool powerOk = false;
    const double freqValue = parseFirstDouble(freqReply.text, &freqOk);
    const double powerValue = parseFirstDouble(powerReply.text, &powerOk);
    if (!freqOk || !powerOk) {
        return {};
    }

    Analyzer4071PeakResult result;
    result.ok = true;
    result.peakFrequencyMHz = freqValue > 1.0e6 ? freqValue / 1.0e6 : freqValue;
    result.peakPowerDbm = powerValue;
    return result;
}

bool Analyzer4071::stopMeasurement()
{
    return stopMeasurement(ScpiRequestOptions());
}

bool Analyzer4071::stopMeasurement(const ScpiRequestOptions &options)
{
    const ScpiWriteResult abortResult =
        m_session->send(QStringLiteral(":ABORt"), options);
    const ScpiWriteResult continuousResult =
        m_session->send(QStringLiteral(":INITiate:CONTinuous OFF"), options);
    m_session->waitForBytesWritten(500);
    return abortResult.isSuccess() && continuousResult.isSuccess();
}

double Analyzer4071::parseFirstDouble(const QString &text, bool *ok) const
{
    if (ok) {
        *ok = false;
    }

    QString normalized = text;
    normalized.replace(';', ',');
    normalized.replace('\n', ',');
    normalized.replace('\r', ',');
    const QStringList parts = normalized.split(',',
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
                                               Qt::SkipEmptyParts
#else
                                               QString::SkipEmptyParts
#endif
                                               );
    for (QString part : parts) {
        part = part.trimmed();
        bool valueOk = false;
        const double value = part.toDouble(&valueOk);
        if (valueOk) {
            if (ok) {
                *ok = true;
            }
            return value;
        }
    }
    return 0.0;
}
