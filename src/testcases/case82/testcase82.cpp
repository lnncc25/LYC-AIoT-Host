#include "testcase82.h"

#include "analyzer4071.h"
#include "case82cancellationtoken.h"
#include "case82constants.h"
#include "generator1466.h"
#include "icase82resultpresenter.h"
#include "itesteventsink.h"

#include <QElapsedTimer>
#include <QThread>
#include <QtGlobal>

TestCase82::TestCase82(Analyzer4071 *analyzer,
                       Generator1466 *generator,
                       const Case82RunConfig &config,
                       ITestEventSink *eventSink,
                       ICase82ResultPresenter *presenter,
                       Case82CancellationToken *cancellationToken)
    : m_analyzer(analyzer)
    , m_generator(generator)
    , m_config(config)
    , m_eventSink(eventSink)
    , m_presenter(presenter)
    , m_cancellationToken(cancellationToken)
{
}

TestCaseId TestCase82::id() const
{
    return TestCaseId::Case82;
}

QString TestCase82::displayName() const
{
    return QStringLiteral("8.2 输出功率");
}

bool TestCase82::canStart() const
{
    return m_analyzer && m_generator && m_config.isValid();
}

TestCompletion TestCase82::start()
{
    if (!canStart()) {
        return {CompletionReason::ExecutionFailed,
                TestVerdict::NotRun,
                QStringLiteral("8.2 configuration invalid")};
    }
    if (isCancellationRequested()) {
        return {CompletionReason::Cancelled,
                TestVerdict::NotRun,
                QStringLiteral("stop requested")};
    }

    m_presenter->presentCase82Initial(m_config);
    addLog(QStringLiteral("INFO"), QStringLiteral("联调"),
           QStringLiteral("8.2 开始真实仪表联调: %1 个功率点，每点 %2 次采样")
           .arg(m_config.powerPoints.size())
           .arg(Case82Constants::SampleCount));

    Case82Result result;
    result.generatorIdn = m_generator->identify(1500).text;
    result.analyzerIdn = m_analyzer->identify(1500).text;
    const bool idnOk = !result.generatorIdn.isEmpty()
            && !result.analyzerIdn.isEmpty();

    QElapsedTimer trendTimer;
    trendTimer.start();

    for (int row = 0; row < m_config.powerPoints.size(); ++row) {
        if (isCancellationRequested()) {
            m_presenter->presentCase82Result(result);
            return {CompletionReason::Cancelled,
                    TestVerdict::NotRun,
                    QStringLiteral("stop requested")};
        }

        const double targetPowerDbm = m_config.powerPoints[row];
        Case82RowResult rowResult;
        rowResult.targetPowerDbm = targetPowerDbm;
        bool rowAnalyzerLimitTriggered = false;
        QVector<double> pointPowers;
        QVector<double> pointFreqs;

        if (!idnOk) {
            rowResult.failureReason = QStringLiteral("仪表 *IDN? 未完整返回");
        } else if (targetPowerDbm > Case82Constants::AnalyzerLimitDbm) {
            rowAnalyzerLimitTriggered = true;
            result.analyzerLimitTriggered = true;
            rowResult.failureReason = QStringLiteral("预计 4071 输入 %1 dBm 超过保护线 %2 dBm")
                    .arg(targetPowerDbm, 0, 'f', 1)
                    .arg(Case82Constants::AnalyzerLimitDbm, 0, 'f', 1);
        } else {
            const Generator1466CwConfigResult gen =
                    m_generator->configureCw(m_config.frequencyMHz, targetPowerDbm, true, false);
            rowResult.generatorConfigured = gen.ok;
            addLog(gen.ok ? QStringLiteral("PASS") : QStringLiteral("WARN"),
                   QStringLiteral("1466"),
                   QStringLiteral("CW配置完成: freq=%1, power=%2, output=%3, err=%4")
                   .arg(gen.frequencyResponse.isEmpty()
                        ? QString::number(m_config.frequencyMHz, 'f', 3) + QStringLiteral(" MHz")
                        : gen.frequencyResponse)
                   .arg(gen.powerResponse.isEmpty()
                        ? QString::number(gen.outputPowerDbm, 'f', 1) + QStringLiteral(" dBm")
                        : gen.powerResponse)
                   .arg(gen.outputResponse.isEmpty()
                        ? QStringLiteral("ON")
                        : gen.outputResponse)
                   .arg(gen.errorResponse.isEmpty()
                        ? QStringLiteral("未返回")
                        : gen.errorResponse));
            if (isCancellationRequested()) {
                m_presenter->presentCase82Result(result);
                return {CompletionReason::Cancelled,
                        TestVerdict::NotRun,
                        QStringLiteral("stop requested")};
            }

            if (rowResult.generatorConfigured) {
                if (isCancellationRequested()) {
                    m_presenter->presentCase82Result(result);
                    return {CompletionReason::Cancelled,
                            TestVerdict::NotRun,
                            QStringLiteral("stop requested")};
                }
                const Analyzer4071SpectrumConfigResult spectrum =
                        m_analyzer->configureSpectrum(m_config.frequencyMHz,
                                                      m_config.spanMHz,
                                                      Case82Constants::RbwKHz,
                                                      Case82Constants::VbwKHz);
                rowResult.analyzerConfigured = spectrum.ok;
                addLog(QStringLiteral("INFO"), QStringLiteral("4071"),
                       QStringLiteral("频谱配置完成: center=%1 MHz, span=%2 MHz, RBW=%3 kHz, VBW=%4 kHz, err=%5")
                       .arg(m_config.frequencyMHz, 0, 'f', 3)
                       .arg(m_config.spanMHz, 0, 'f', 3)
                       .arg(Case82Constants::RbwKHz, 0, 'f', 1)
                       .arg(Case82Constants::VbwKHz, 0, 'f', 1)
                       .arg(spectrum.errorResponse.isEmpty()
                            ? QStringLiteral("未返回")
                            : spectrum.errorResponse));
                if (isCancellationRequested()) {
                    m_presenter->presentCase82Result(result);
                    return {CompletionReason::Cancelled,
                            TestVerdict::NotRun,
                            QStringLiteral("stop requested")};
                }
            }

            if (rowResult.generatorConfigured && rowResult.analyzerConfigured) {
                for (int sample = 0; sample < Case82Constants::SampleCount; ++sample) {
                    if (isCancellationRequested()) {
                        m_presenter->presentCase82Result(result);
                        return {CompletionReason::Cancelled,
                                TestVerdict::NotRun,
                                QStringLiteral("stop requested")};
                    }

                    const Analyzer4071PeakResult peak = m_analyzer->readPeak();
                    if (peak.ok) {
                        pointFreqs.append(peak.peakFrequencyMHz);
                        pointPowers.append(peak.peakPowerDbm);
                        Case82Sample sampleValue;
                        sampleValue.timeSec = trendTimer.elapsed() / 1000.0;
                        sampleValue.measuredPowerDbm = peak.peakPowerDbm;
                        sampleValue.targetPowerDbm = targetPowerDbm;
                        result.trendTimeSec.append(sampleValue.timeSec);
                        result.trendMeasuredPowerDbm.append(sampleValue.measuredPowerDbm);
                        result.trendTargetPowerDbm.append(sampleValue.targetPowerDbm);
                        m_presenter->presentCase82Sample(sampleValue);
                        if (peak.peakPowerDbm > Case82Constants::AnalyzerLimitDbm) {
                            rowAnalyzerLimitTriggered = true;
                            result.analyzerLimitTriggered = true;
                            rowResult.failureReason =
                                    QStringLiteral("4071 实测输入 %1 dBm 超过保护线 %2 dBm")
                                    .arg(peak.peakPowerDbm, 0, 'f', 2)
                                    .arg(Case82Constants::AnalyzerLimitDbm, 0, 'f', 1);
                            break;
                        }
                    }
                    if (!cancellableSleep(Case82Constants::SampleIntervalMs)) {
                        m_presenter->presentCase82Result(result);
                        return {CompletionReason::Cancelled,
                                TestVerdict::NotRun,
                                QStringLiteral("stop requested")};
                    }
                }
            }
        }

        for (double value : pointPowers) {
            rowResult.averagePowerDbm += value;
        }
        for (double value : pointFreqs) {
            rowResult.averageFrequencyMHz += value;
        }
        rowResult.peakOk = !pointPowers.isEmpty()
                && pointPowers.size() == pointFreqs.size();
        rowResult.sampleCount = pointPowers.size();
        if (rowResult.peakOk) {
            rowResult.averagePowerDbm /= pointPowers.size();
            rowResult.averageFrequencyMHz /= pointFreqs.size();
        }

        rowResult.powerErrorDb = rowResult.peakOk
                ? rowResult.averagePowerDbm - targetPowerDbm
                : 0.0;
        rowResult.frequencyErrorKHz = rowResult.peakOk
                ? (rowResult.averageFrequencyMHz - m_config.frequencyMHz) * 1000.0
                : 0.0;
        const bool powerPass = rowResult.peakOk
                && qAbs(rowResult.powerErrorDb) <= Case82Constants::PowerToleranceDb;
        const bool freqPass = rowResult.peakOk
                && qAbs(rowResult.frequencyErrorKHz) <= 5.0;
        rowResult.pass = idnOk
                && rowResult.generatorConfigured
                && rowResult.analyzerConfigured
                && powerPass
                && freqPass
                && !rowAnalyzerLimitTriggered;

        if (rowResult.failureReason.isEmpty() && !rowResult.generatorConfigured) {
            rowResult.failureReason = QStringLiteral("1466 配置失败");
        }
        if (rowResult.failureReason.isEmpty() && !rowResult.analyzerConfigured) {
            rowResult.failureReason = QStringLiteral("4071 配置失败");
        }
        if (rowResult.failureReason.isEmpty() && !rowResult.peakOk) {
            rowResult.failureReason = QStringLiteral("4071 未返回有效采样");
        }
        if (rowResult.failureReason.isEmpty() && !freqPass) {
            rowResult.failureReason = QStringLiteral("频率误差超限");
        }
        if (rowResult.failureReason.isEmpty() && !powerPass) {
            rowResult.failureReason = QStringLiteral("功率误差超限");
        }

        result.maxAbsPowerErrorDb = qMax(result.maxAbsPowerErrorDb,
                                        qAbs(rowResult.powerErrorDb));
        if (rowResult.pass) {
            ++result.passCount;
        } else {
            ++result.failCount;
        }
        result.rows.append(rowResult);
        m_presenter->presentCase82Row(row, rowResult);
        addLog(rowResult.pass ? QStringLiteral("PASS") : QStringLiteral("FAIL"),
               QStringLiteral("8.2"),
               QStringLiteral("功率点 %1 dBm: avgPower=%2 dBm, powerErr=%3 dB, samples=%4/%5, %6")
               .arg(targetPowerDbm, 0, 'f', 2)
               .arg(rowResult.peakOk ? rowResult.averagePowerDbm : 0.0, 0, 'f', 2)
               .arg(rowResult.peakOk ? rowResult.powerErrorDb : 0.0, 0, 'f', 2)
               .arg(rowResult.sampleCount)
               .arg(Case82Constants::SampleCount)
               .arg(rowResult.pass ? QStringLiteral("PASS") : rowResult.failureReason));

        if (rowAnalyzerLimitTriggered) {
            break;
        }
    }

    result.overallPass = idnOk
            && result.failCount == 0
            && result.passCount == m_config.powerPoints.size();
    m_presenter->presentCase82Result(result);
    addLog(result.overallPass ? QStringLiteral("PASS") : QStringLiteral("FAIL"),
           QStringLiteral("联调"),
           QStringLiteral("8.2 真实联调完成: points=%1, pass=%2, fail=%3, 1466=%4, 4071=%5")
           .arg(m_config.powerPoints.size())
           .arg(result.passCount)
           .arg(result.failCount)
           .arg(result.generatorIdn.isEmpty() ? QStringLiteral("未返回") : result.generatorIdn)
           .arg(result.analyzerIdn.isEmpty() ? QStringLiteral("未返回") : result.analyzerIdn));
    return {CompletionReason::Completed,
            result.overallPass ? TestVerdict::Pass : TestVerdict::Fail,
            QString()};
}

void TestCase82::requestStop()
{
    if (m_cancellationToken) {
        m_cancellationToken->requestCancellation();
    }
}

bool TestCase82::isCancellationRequested() const
{
    return m_cancellationToken && m_cancellationToken->isCancellationRequested();
}

bool TestCase82::cancellableSleep(int milliseconds) const
{
    for (int elapsed = 0; elapsed < milliseconds; elapsed += 20) {
        if (isCancellationRequested()) {
            return false;
        }
        QThread::msleep(qMin(20, milliseconds - elapsed));
    }
    return !isCancellationRequested();
}

void TestCase82::addLog(const QString &level,
                        const QString &source,
                        const QString &message)
{
    if (m_eventSink) {
        m_eventSink->addTestLog(level, source, message);
    }
}
