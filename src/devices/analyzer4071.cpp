#include "analyzer4071.h"

#include "scpi/instrumentsession.h"

#include <QThread>
#include <QVector>
#include <QtGlobal>
#include <cmath>

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

Analyzer4071AclrConfigResult Analyzer4071::configureAclr(double centerFreqMHz,
                                                         double carrierBWKHz,
                                                         double integrationBWKHz,
                                                         double offsetKHz)
{
    m_session->send(QStringLiteral("*CLS"));
    m_session->send(QStringLiteral(":CONFigure:ACPower"));
    QThread::msleep(50);
    m_session->send(QStringLiteral(":SENSe:FREQuency:CENTer %1MHz").arg(centerFreqMHz, 0, 'f', 6));
    const double spanHalfWidthKHz = offsetKHz + qMax(carrierBWKHz, integrationBWKHz);
    double totalSpanMHz = spanHalfWidthKHz * 2.0 / 1000.0;
    totalSpanMHz = qMax(totalSpanMHz, 5.0);
    m_session->send(QStringLiteral(":SENSe:ACPower:FREQuency:SPAN %1MHz").arg(totalSpanMHz, 0, 'f', 6));
    m_session->send(QStringLiteral(":SENSe:ACPower:CARRier:COUNt 1"));
    m_session->send(QStringLiteral(":SENSe:ACPower:MODE NORMal"));
    m_session->send(QStringLiteral(":SENSe:ACPower:TYPE TPRef"));
    m_session->send(QStringLiteral(":SENSe:ACPower:CARRier:LIST:BANDwidth:INTegration %1Hz")
                    .arg(carrierBWKHz * 1000.0, 0, 'f', 0));
    m_session->send(QStringLiteral(":SENSe:ACPower:OFFSet:LIST:FREQuency %1Hz")
                    .arg(offsetKHz * 1000.0, 0, 'f', 0));
    m_session->send(QStringLiteral(":SENSe:ACPower:OFFSet:LIST:BANDwidth:INTegration %1Hz")
                    .arg(integrationBWKHz * 1000.0, 0, 'f', 0));
    m_session->send(QStringLiteral(":SENSe:ACPower:OFFSet:LIST:BANDwidth:SHAPe FLATtop"));
    m_session->send(QStringLiteral(":SENSe:ACPower:METHod IBW"));

    Analyzer4071AclrConfigResult result;
    result.errorQueue = drainErrorQueue();
    result.ok = true;
    for (const QString &error : result.errorQueue) {
        if (!scpiErrorIsClear(error)) {
            result.ok = false;
            break;
        }
    }
    return result;
}

Analyzer4071AclrMeasurementResult Analyzer4071::measureAclr()
{
    m_session->send(QStringLiteral(":INITiate:CONTinuous OFF"));
    m_session->send(QStringLiteral(":INITiate:IMMediate"));
    m_session->query(QStringLiteral("*OPC?"), 3000);

    Analyzer4071AclrMeasurementResult result;
    result.response = m_session->query(QStringLiteral(":READ:ACPower?"), 2000).text;
    if (result.response.isEmpty()) {
        result.details = QStringLiteral("empty response");
        return result;
    }

    result.ok = parseAcpowerResponse(result.response,
                                     result.mainPowerDbm,
                                     result.leftAclrDb,
                                     result.rightAclrDb,
                                     &result.details);
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

Analyzer4071FileResult Analyzer4071::saveScreenSnapshot(const QString &fileName)
{
    const QString trimmedName = fileName.trimmed();
    Analyzer4071FileResult result;
    if (trimmedName.isEmpty()) {
        result.errorQueue << QStringLiteral("empty filename");
        return result;
    }

    m_session->send(QStringLiteral(":MMEMory:STORe:SCReen \"%1\"").arg(trimmedName));
    QThread::msleep(200);
    result.errorQueue = drainErrorQueue();
    result.ok = true;
    for (const QString &error : result.errorQueue) {
        if (!scpiErrorIsClear(error)) {
            result.ok = false;
            break;
        }
    }
    return result;
}

Analyzer4071BinaryFileResult Analyzer4071::downloadFile(const QString &remoteFileName,
                                                        int timeoutMs)
{
    const BinaryBlockReply reply =
        m_session->queryBinaryBlock(QStringLiteral(":MMEMory:DATA? \"%1\"").arg(remoteFileName),
                                    timeoutMs);
    return {reply.isSuccess(), reply.payload, reply.error};
}

Analyzer4071FileResult Analyzer4071::deleteFile(const QString &remoteFileName)
{
    m_session->send(QStringLiteral(":MMEMory:DELete \"%1\"").arg(remoteFileName));
    Analyzer4071FileResult result;
    result.errorQueue = drainErrorQueue();
    result.ok = true;
    for (const QString &error : result.errorQueue) {
        if (!scpiErrorIsClear(error)) {
            result.ok = false;
            break;
        }
    }
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

bool Analyzer4071::parseAcpowerResponse(const QString &response,
                                        double &mainPowerDbm,
                                        double &leftAclrDb,
                                        double &rightAclrDb,
                                        QString *details) const
{
    mainPowerDbm = 0.0;
    leftAclrDb = 0.0;
    rightAclrDb = 0.0;

    QString normalized = response;
    normalized.replace(';', ',');
    normalized.replace('\n', ',');
    normalized.replace('\r', ',');
    const QStringList tokens = normalized.split(',',
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
                                               Qt::SkipEmptyParts
#else
                                               QString::SkipEmptyParts
#endif
                                               );
    QVector<double> values;
    values.reserve(tokens.size());

    for (const QString &token : tokens) {
        bool ok = false;
        const double value = token.trimmed().toDouble(&ok);
        if (!ok || !std::isfinite(value)) {
            continue;
        }
        values.push_back(value);
    }

    auto setDetails = [details](const QString &text) {
        if (details) {
            *details = text;
        }
    };

    auto looksLikeRelativeDb = [](double value) {
        return value < 0.0 && value > -200.0;
    };
    auto looksLikeAclrDb = [](double value) {
        return value >= 0.0 && value <= 120.0;
    };

    if (values.size() >= 6 && qAbs(values[0] - values[1]) < 1.0) {
        mainPowerDbm = values[0];
        leftAclrDb = values[0] - values[3];
        rightAclrDb = values[0] - values[5];
        if (looksLikeRelativeDb(values[2])) {
            leftAclrDb = -values[2];
        }
        if (looksLikeRelativeDb(values[4])) {
            rightAclrDb = -values[4];
        }
        setDetails(QStringLiteral("parsed as extended ACP tuple: main=%1, leftAclr=%2, rightAclr=%3")
                   .arg(mainPowerDbm, 0, 'f', 4)
                   .arg(leftAclrDb, 0, 'f', 4)
                   .arg(rightAclrDb, 0, 'f', 4));
        return true;
    }

    if (values.size() >= 5 && qAbs(values[0] - values[1]) >= 1.0) {
        const double candidateMain = values[0];
        const double candidateLeftPower = values[1];
        const double candidateRightPower = values[2];
        mainPowerDbm = candidateMain;
        leftAclrDb = candidateMain - candidateLeftPower;
        rightAclrDb = candidateMain - candidateRightPower;
        setDetails(QStringLiteral("parsed as legacy power list: main=%1, leftPower=%2, rightPower=%3")
                   .arg(mainPowerDbm, 0, 'f', 4)
                   .arg(candidateLeftPower, 0, 'f', 4)
                   .arg(candidateRightPower, 0, 'f', 4));
        return true;
    }

    if (values.size() >= 3) {
        mainPowerDbm = values[0];
        if (looksLikeRelativeDb(values[1]) && looksLikeRelativeDb(values[2])) {
            leftAclrDb = -values[1];
            rightAclrDb = -values[2];
            setDetails(QStringLiteral("parsed as relative ACLR tuple: main=%1, leftAclr=%2, rightAclr=%3")
                       .arg(mainPowerDbm, 0, 'f', 4)
                       .arg(leftAclrDb, 0, 'f', 4)
                       .arg(rightAclrDb, 0, 'f', 4));
        } else if (looksLikeAclrDb(values[1]) && looksLikeAclrDb(values[2])) {
            leftAclrDb = values[1];
            rightAclrDb = values[2];
            setDetails(QStringLiteral("parsed as direct ACLR tuple: main=%1, leftAclr=%2, rightAclr=%3")
                       .arg(mainPowerDbm, 0, 'f', 4)
                       .arg(leftAclrDb, 0, 'f', 4)
                       .arg(rightAclrDb, 0, 'f', 4));
        } else {
            leftAclrDb = values[0] - values[1];
            rightAclrDb = values[0] - values[2];
            setDetails(QStringLiteral("parsed as compact power list: main=%1, leftAclr=%2, rightAclr=%3")
                       .arg(mainPowerDbm, 0, 'f', 4)
                       .arg(leftAclrDb, 0, 'f', 4)
                       .arg(rightAclrDb, 0, 'f', 4));
        }
        return true;
    }

    setDetails(QStringLiteral("unable to parse ACPower response, numericCount=%1").arg(values.size()));
    return false;
}

QStringList Analyzer4071::drainErrorQueue(int maxReads)
{
    QStringList values;
    for (int index = 0; index < maxReads; ++index) {
        const QString error = m_session->query(QStringLiteral(":SYSTem:ERRor?"), 1200).text.trimmed();
        values.append(error);
        if (scpiErrorIsClear(error)) {
            break;
        }
    }
    return values;
}
