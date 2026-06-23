#include "testcase85.h"

#include "analyzer4071.h"
#include "case85cancellationtoken.h"
#include "generator1466.h"
#include "icase85resultpresenter.h"
#include "itesteventsink.h"
#include "outputpaths.h"

#include <QDir>
#include <QFile>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QThread>

namespace {

constexpr double kCase85ArbClockMHz = 1.92;

QString formatOrDash(bool valid, double value)
{
    return valid ? QString::number(value, 'f', 2) : QStringLiteral("--");
}

}

TestCase85::TestCase85(Analyzer4071 *analyzer,
                       Generator1466 *generator,
                       const Case85RunConfig &config,
                       ITestEventSink *eventSink,
                       ICase85ResultPresenter *presenter,
                       Case85CancellationToken *cancellationToken)
    : m_analyzer(analyzer)
    , m_generator(generator)
    , m_config(config)
    , m_eventSink(eventSink)
    , m_presenter(presenter)
    , m_cancellationToken(cancellationToken)
{
}

TestCaseId TestCase85::id() const
{
    return TestCaseId::Case85;
}

QString TestCase85::displayName() const
{
    return QStringLiteral("8.5 ACLR");
}

bool TestCase85::canStart() const
{
    if (!m_config.isValid()) {
        return false;
    }
    if (!m_config.realInstrumentMode) {
        return true;
    }
    return m_analyzer && m_generator;
}

TestCompletion TestCase85::start()
{
    if (!canStart()) {
        return {CompletionReason::ExecutionFailed,
                TestVerdict::NotRun,
                QStringLiteral("8.5 configuration invalid")};
    }
    if (isCancellationRequested()) {
        return {CompletionReason::Cancelled,
                TestVerdict::NotRun,
                QStringLiteral("stop requested")};
    }

    Case85Result result;
    result.realInstrumentMode = m_config.realInstrumentMode;
    result.screenshotRunDir = m_config.screenshotRunDir;
    m_presenter->presentCase85Initial(totalRowCount());

    if (m_config.realInstrumentMode && !m_config.screenshotRunDir.isEmpty()) {
        QDir().mkpath(m_config.screenshotRunDir);
        addLog(QStringLiteral("INFO"), QStringLiteral("上位机"),
               QStringLiteral("8.5 截图本地目录: %1").arg(m_config.screenshotRunDir));
    }

    bool overallPass = true;
    bool hasValidResult = false;
    int row = 0;

    for (const Case85BandwidthConfig &cfg : m_config.bandwidthConfigs) {
        if (isCancellationRequested()) {
            m_presenter->presentCase85Result(result);
            return {CompletionReason::Cancelled, TestVerdict::NotRun, QStringLiteral("stop requested")};
        }

        const QList<double> freqPoints = frequencyPointsForBandwidth(cfg.channelBWKHz);
        QStringList freqText;
        for (double value : freqPoints) {
            freqText << QString::number(value, 'f', 3);
        }
        addLog(QStringLiteral("INFO"), QStringLiteral("8.5"),
               QStringLiteral("带宽 %1 kHz: 将测试 %2 个频点: %3")
               .arg(cfg.channelBWKHz)
               .arg(freqPoints.size())
               .arg(freqText.join(QStringLiteral(", "))));

        for (double centerFreqMHz : freqPoints) {
            if (isCancellationRequested()) {
                m_presenter->presentCase85Result(result);
                return {CompletionReason::Cancelled, TestVerdict::NotRun, QStringLiteral("stop requested")};
            }

            bool waveformLoaded = true;
            if (m_config.realInstrumentMode) {
                addLog(QStringLiteral("INFO"), QStringLiteral("1466"),
                       QStringLiteral("频点 %1 MHz: 加载 %2 kHz A-TM1.1 测试模型文件 %3")
                       .arg(centerFreqMHz, 0, 'f', 3)
                       .arg(cfg.channelBWKHz)
                       .arg(cfg.iqFileName));
                const Generator1466ArbConfigResult arb =
                    m_generator->configureArbPlayback(cfg.iqFileName,
                                                      centerFreqMHz,
                                                      m_config.txPowerDbm,
                                                      kCase85ArbClockMHz);
                waveformLoaded = arb.ok;
                if (waveformLoaded) {
                    QThread::msleep(500);
                } else {
                    addLog(QStringLiteral("ERROR"), QStringLiteral("1466"),
                           QStringLiteral("频点 %1 MHz: 波形加载失败，跳过该带宽后续测量")
                           .arg(centerFreqMHz, 0, 'f', 3));
                    break;
                }
                if (isCancellationRequested()) {
                    m_presenter->presentCase85Result(result);
                    return {CompletionReason::Cancelled, TestVerdict::NotRun, QStringLiteral("stop requested")};
                }
            }

            for (const Case85OffsetConfig &offsetCfg : cfg.offsets) {
                if (isCancellationRequested()) {
                    m_presenter->presentCase85Result(result);
                    return {CompletionReason::Cancelled, TestVerdict::NotRun, QStringLiteral("stop requested")};
                }

                Case85RowResult rowResult;
                rowResult.row = row;
                rowResult.channelBWKHz = cfg.channelBWKHz;
                rowResult.centerFreqMHz = centerFreqMHz;
                rowResult.offsetKHz = offsetCfg.offsetKHz;
                rowResult.integrationBWKHz = offsetCfg.integrationBWKHz;
                rowResult.limitDb = offsetCfg.limitDb;
                rowResult.detail.simImagePath = QStringLiteral(":/images/sim/BW%1k.png").arg(cfg.channelBWKHz);
                if (!QFile::exists(rowResult.detail.simImagePath)) {
                    rowResult.detail.simImagePath.clear();
                }

                if (m_config.realInstrumentMode) {
                    if (!waveformLoaded) {
                        rowResult.failureReason = QStringLiteral("IQ文件加载失败");
                    } else {
                        const Analyzer4071AclrConfigResult aclrConfig =
                            m_analyzer->configureAclr(centerFreqMHz,
                                                      cfg.channelBWKHz,
                                                      offsetCfg.integrationBWKHz,
                                                      offsetCfg.offsetKHz);
                        if (!aclrConfig.ok) {
                            rowResult.failureReason = QStringLiteral("ACLR配置失败");
                        } else if (isCancellationRequested()) {
                            m_presenter->presentCase85Result(result);
                            return {CompletionReason::Cancelled, TestVerdict::NotRun, QStringLiteral("stop requested")};
                        } else {
                            const Analyzer4071AclrMeasurementResult aclr =
                                m_analyzer->measureAclr();
                            if (!aclr.ok) {
                                rowResult.failureReason = QStringLiteral("ACLR读取失败");
                            } else {
                                rowResult.mainPowerDbm = aclr.mainPowerDbm;
                                rowResult.leftAclrDb = aclr.leftAclrDb;
                                rowResult.rightAclrDb = aclr.rightAclrDb;
                                rowResult.leftPowerDbm = aclr.mainPowerDbm - aclr.leftAclrDb;
                                rowResult.rightPowerDbm = aclr.mainPowerDbm - aclr.rightAclrDb;
                                rowResult.measurementOk = true;
                                hasValidResult = true;
                            }
                        }
                    }
                } else {
                    const double randomLeft =
                        (QRandomGenerator::global()->bounded(20) - 10) / 10.0;
                    const double randomRight =
                        (QRandomGenerator::global()->bounded(20) - 10) / 10.0;
                    rowResult.mainPowerDbm = 20.0;
                    rowResult.leftAclrDb = offsetCfg.limitDb + 2.5 + randomLeft;
                    rowResult.rightAclrDb = offsetCfg.limitDb + 2.5 + randomRight;
                    rowResult.leftPowerDbm = rowResult.mainPowerDbm - rowResult.leftAclrDb;
                    rowResult.rightPowerDbm = rowResult.mainPowerDbm - rowResult.rightAclrDb;
                    rowResult.measurementOk = true;
                    hasValidResult = true;
                }

                rowResult.pass = rowResult.measurementOk
                        && rowResult.leftAclrDb >= offsetCfg.limitDb
                        && rowResult.rightAclrDb >= offsetCfg.limitDb;
                rowResult.verdict = rowResult.pass ? QStringLiteral("PASS")
                                                   : QStringLiteral("FAIL");
                overallPass = overallPass && rowResult.pass;

                rowResult.testPointDesc =
                    QStringLiteral("%1 MHz / %2 kHz / ±%3 kHz")
                    .arg(centerFreqMHz, 0, 'f', 3)
                    .arg(cfg.channelBWKHz)
                    .arg(offsetCfg.offsetKHz, 0, 'f', 0);
                rowResult.configDesc =
                    QStringLiteral("IBW:%1 kHz")
                    .arg(offsetCfg.integrationBWKHz, 0, 'f', 0);
                rowResult.measurementDesc = rowResult.measurementOk
                        ? QStringLiteral("L:%1 dB, R:%2 dB")
                          .arg(rowResult.leftAclrDb, 0, 'f', 2)
                          .arg(rowResult.rightAclrDb, 0, 'f', 2)
                        : QStringLiteral("失败");
                rowResult.itemName = qFuzzyCompare(offsetCfg.limitDb, 40.8)
                        ? QStringLiteral("第一邻道ACLR")
                        : QStringLiteral("第二邻道ACLR");
                rowResult.settingValue =
                    QStringLiteral("IBW:%1 kHz")
                    .arg(offsetCfg.integrationBWKHz, 0, 'f', 0);
                rowResult.standardValue =
                    QStringLiteral("≥%1 dB")
                    .arg(offsetCfg.limitDb, 0, 'f', 1);

                if (rowResult.measurementOk && m_config.realInstrumentMode) {
                    const QString snapshotName = OutputPaths::timestampedScreenshotName(
                        QStringLiteral("ACLR_%1MHz_BW%2k_OFF%3k_IBW%4k_%5")
                            .arg(centerFreqMHz, 0, 'f', 3)
                            .arg(cfg.channelBWKHz)
                            .arg(offsetCfg.offsetKHz, 0, 'f', 0)
                            .arg(offsetCfg.integrationBWKHz, 0, 'f', 0)
                            .arg(rowResult.pass ? QStringLiteral("PASS")
                                                : QStringLiteral("FAIL")));
                    const QString localSnapshotPath =
                        QDir(m_config.screenshotRunDir).filePath(snapshotName);
                    const Analyzer4071FileResult saveResult =
                        m_analyzer->saveScreenSnapshot(snapshotName);
                    if (saveResult.ok) {
                        const Analyzer4071BinaryFileResult downloadResult =
                            m_analyzer->downloadFile(snapshotName);
                        if (downloadResult.ok) {
                            QFile localFile(localSnapshotPath);
                            if (localFile.open(QIODevice::WriteOnly)) {
                                const qint64 bytesWritten = localFile.write(downloadResult.payload);
                                const bool flushed = localFile.flush();
                                localFile.close();
                                if (bytesWritten == downloadResult.payload.size()
                                        && flushed
                                        && localFile.error() == QFileDevice::NoError) {
                                    rowResult.detail.screenshotPath = localSnapshotPath;
                                    m_analyzer->deleteFile(snapshotName);
                                } else {
                                    addLog(QStringLiteral("ERROR"), QStringLiteral("4071"),
                                           QStringLiteral("截图写入本地失败，保留仪表端文件待人工检查: %1")
                                           .arg(localSnapshotPath));
                                }
                            } else {
                                addLog(QStringLiteral("ERROR"), QStringLiteral("4071"),
                                       QStringLiteral("截图本地文件打开失败，保留仪表端文件待人工检查: %1")
                                       .arg(localSnapshotPath));
                            }
                        }
                    }
                    if (isCancellationRequested()) {
                        m_presenter->presentCase85Result(result);
                        return {CompletionReason::Cancelled, TestVerdict::NotRun, QStringLiteral("stop requested")};
                    }
                }

                rowResult.detail.statusText = QStringLiteral(
                    "测试点: %1\n"
                    "测量项目: %2\n"
                    "设定值: %3\n"
                    "主信道功率: %4 dBm\n"
                    "左邻道功率: %5 dBm\n"
                    "右邻道功率: %6 dBm\n"
                    "测量值: 左ACLR = %7 dB, 右ACLR = %8 dB\n"
                    "判定: %9")
                    .arg(rowResult.testPointDesc)
                    .arg(rowResult.itemName)
                    .arg(rowResult.settingValue)
                    .arg(formatOrDash(rowResult.measurementOk, rowResult.mainPowerDbm))
                    .arg(formatOrDash(rowResult.measurementOk, rowResult.leftPowerDbm))
                    .arg(formatOrDash(rowResult.measurementOk, rowResult.rightPowerDbm))
                    .arg(formatOrDash(rowResult.measurementOk, rowResult.leftAclrDb))
                    .arg(formatOrDash(rowResult.measurementOk, rowResult.rightAclrDb))
                    .arg(rowResult.verdict);

                if (!rowResult.failureReason.isEmpty()) {
                    addLog(QStringLiteral("FAIL"), QStringLiteral("8.5"),
                           QStringLiteral("频点 %1 MHz, 带宽 %2 kHz, 邻道偏移±%3 kHz: %4")
                           .arg(centerFreqMHz, 0, 'f', 3)
                           .arg(cfg.channelBWKHz)
                           .arg(offsetCfg.offsetKHz, 0, 'f', 0)
                           .arg(rowResult.failureReason));
                } else {
                    addLog(rowResult.pass ? QStringLiteral("PASS") : QStringLiteral("FAIL"),
                           QStringLiteral("8.5"),
                           QStringLiteral("频点 %1 MHz, 带宽 %2 kHz, 邻道偏移±%3 kHz, 滤波带宽%4 kHz: 左ACLR=%5 dB, 右ACLR=%6 dB, 限值=%7 dB")
                           .arg(centerFreqMHz, 0, 'f', 3)
                           .arg(cfg.channelBWKHz)
                           .arg(offsetCfg.offsetKHz, 0, 'f', 0)
                           .arg(offsetCfg.integrationBWKHz, 0, 'f', 0)
                           .arg(rowResult.leftAclrDb, 0, 'f', 2)
                           .arg(rowResult.rightAclrDb, 0, 'f', 2)
                           .arg(offsetCfg.limitDb, 0, 'f', 1));
                }

                if (rowResult.pass) {
                    ++result.passCount;
                } else {
                    ++result.failCount;
                }
                result.rows.append(rowResult);
                m_presenter->presentCase85Row(row, rowResult);
                ++row;
                QThread::msleep(10);
            }
            QThread::msleep(200);
        }
    }

    result.hasValidResult = hasValidResult;
    result.overallPass = hasValidResult && overallPass;
    m_presenter->presentCase85Result(result);
    if (isCancellationRequested()) {
        return {CompletionReason::Cancelled, TestVerdict::NotRun, QStringLiteral("stop requested")};
    }

    return {CompletionReason::Completed,
            result.overallPass ? TestVerdict::Pass : TestVerdict::Fail,
            QString()};
}

void TestCase85::requestStop()
{
    if (m_cancellationToken) {
        m_cancellationToken->requestCancellation();
    }
}

bool TestCase85::isCancellationRequested() const
{
    return m_cancellationToken && m_cancellationToken->isCancellationRequested();
}

void TestCase85::addLog(const QString &level,
                        const QString &source,
                        const QString &message)
{
    if (m_eventSink) {
        m_eventSink->addTestLog(level, source, message);
    }
}

QList<double> TestCase85::frequencyPointsForBandwidth(int channelBWKHz) const
{
    QList<double> values;
    const QString rawText = m_config.frequencyTextByBandwidth.value(channelBWKHz, QStringLiteral("925"));
    if (!rawText.isEmpty()) {
        const QStringList parts = rawText.split(QRegularExpression(QStringLiteral("[,\\s]+")),
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
                                                Qt::SkipEmptyParts
#else
                                                QString::SkipEmptyParts
#endif
                                                );
        for (const QString &part : parts) {
            bool ok = false;
            const double value = part.toDouble(&ok);
            if (ok && value > 0.0) {
                values.append(value);
            }
        }
    }
    if (values.isEmpty()) {
        values.append(925.0);
    }
    return values;
}

int TestCase85::totalRowCount() const
{
    int total = 0;
    for (const Case85BandwidthConfig &cfg : m_config.bandwidthConfigs) {
        total += frequencyPointsForBandwidth(cfg.channelBWKHz).size() * cfg.offsets.size();
    }
    return total;
}
