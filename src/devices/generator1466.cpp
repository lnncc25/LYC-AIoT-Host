#include "generator1466.h"

#include "scpi/instrumentsession.h"

#include <QThread>
#include <QtGlobal>

Generator1466::Generator1466(InstrumentSession *session)
    : m_session(session)
{
}

InstrumentSession *Generator1466::session() const
{
    return m_session;
}

bool Generator1466::isConnected() const
{
    return m_session && m_session->isConnected();
}

ScpiReply Generator1466::identify(int timeoutMs)
{
    return m_session->query(QStringLiteral("*IDN?"), timeoutMs);
}

ScpiReply Generator1466::readError(int timeoutMs)
{
    return m_session->query(QStringLiteral(":SYSTem:ERRor?"), timeoutMs);
}

Generator1466CwConfigResult Generator1466::configureCw(double freqMHz,
                                                       double powerDbm,
                                                       bool outputOn,
                                                       bool enforceSafeClamp)
{
    Generator1466CwConfigResult result;
    result.requestedPowerDbm = powerDbm;
    result.outputPowerDbm = enforceSafeClamp ? qBound(-80.0, powerDbm, -10.0) : powerDbm;
    result.clamped = enforceSafeClamp
            && !qFuzzyCompare(result.outputPowerDbm + 100.0, powerDbm + 100.0);

    m_session->send(QStringLiteral("*CLS"));
    QThread::msleep(50);
    m_session->send(QStringLiteral(":SOURce1:FREQuency %1MHz").arg(freqMHz, 0, 'f', 6));
    m_session->send(QStringLiteral(":SOURce1:POWer %1dBm").arg(result.outputPowerDbm, 0, 'f', 2));
    m_session->send(QStringLiteral(":OUTPut1:STATe %1").arg(outputOn ? "ON" : "OFF"));

    result.frequencyResponse = m_session->query(QStringLiteral(":SOURce1:FREQuency?"), 1200).text;
    result.powerResponse = m_session->query(QStringLiteral(":SOURce1:POWer?"), 1200).text;
    result.outputResponse = m_session->query(QStringLiteral(":OUTPut1:STATe?"), 1200).text;
    result.errorResponse = m_session->query(QStringLiteral(":SYSTem:ERRor?"), 1200).text;

    const bool errOk = result.errorResponse.isEmpty()
            || result.errorResponse.startsWith(QStringLiteral("+0"))
            || result.errorResponse.contains(QStringLiteral("No error"), Qt::CaseInsensitive);
    result.ok = !result.frequencyResponse.isEmpty()
            && !result.powerResponse.isEmpty()
            && !result.outputResponse.isEmpty()
            && errOk;
    return result;
}

Generator1466ArbConfigResult Generator1466::configureArbPlayback(const QString &fileName,
                                                                 double freqMHz,
                                                                 double powerDbm,
                                                                 double sampleClockMHz)
{
    m_session->send(QStringLiteral("*CLS"));
    m_session->send(QStringLiteral(":SOURce1:RADio:ARB:WAVeform \"%1\"").arg(fileName));
    if (sampleClockMHz > 0.0) {
        m_session->send(QStringLiteral(":SOURce1:RADio:ARB:SCLock:RATE %1MHz")
                        .arg(sampleClockMHz, 0, 'f', 3));
    }
    m_session->send(QStringLiteral(":SOURce1:FREQuency %1MHz").arg(freqMHz, 0, 'f', 6));
    m_session->send(QStringLiteral(":SOURce1:POWer %1dBm").arg(powerDbm, 0, 'f', 2));
    m_session->send(QStringLiteral(":SOURce1:RADio:ARB:STATe ON"));
    m_session->send(QStringLiteral(":OUTPut1:STATe ON"));
    QThread::msleep(200);

    Generator1466ArbConfigResult result;
    result.sampleClockResponse =
        m_session->query(QStringLiteral(":SOURce1:RADio:ARB:SCLock:RATE?"), 800).text.trimmed();
    result.errorQueue = drainErrorQueue();
    result.ok = true;
    for (const QString &error : result.errorQueue) {
        if (!(error.isEmpty()
              || error.startsWith(QStringLiteral("+0"))
              || error.contains(QStringLiteral("No error"), Qt::CaseInsensitive))) {
            result.ok = false;
            break;
        }
    }
    return result;
}

bool Generator1466::shutdownOutput()
{
    return shutdownOutput(ScpiRequestOptions());
}

bool Generator1466::shutdownOutput(const ScpiRequestOptions &options)
{
    const ScpiWriteResult allOff =
        m_session->send(QStringLiteral(":OUTPut:ALL OFF"), options);
    const ScpiWriteResult channelOff =
        m_session->send(QStringLiteral(":OUTPut1:STATe OFF"), options);
    m_session->waitForBytesWritten(500);
    return allOff.isSuccess() && channelOff.isSuccess();
}

QStringList Generator1466::drainErrorQueue(int maxReads)
{
    QStringList values;
    for (int index = 0; index < maxReads; ++index) {
        const QString error = m_session->query(QStringLiteral(":SYSTem:ERRor?"), 1200).text.trimmed();
        values.append(error);
        if (error.isEmpty()
            || error.startsWith(QStringLiteral("+0"))
            || error.contains(QStringLiteral("No error"), Qt::CaseInsensitive)) {
            break;
        }
    }
    return values;
}
