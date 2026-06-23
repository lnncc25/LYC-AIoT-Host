#include "analyzer4071.h"
#include "case81runconfig.h"
#include "case81runcontroller.h"
#include "case81model.h"
#include "case82constants.h"
#include "case82model.h"
#include "case82powerpoints.h"
#include "case82runcontroller.h"
#include "case85model.h"
#include "case85runcontroller.h"
#include "csvutils.h"
#include "fakescpitransport.h"
#include "generator1466.h"
#include "icase81resultpresenter.h"
#include "instrumentsession.h"
#include "itestcaseview.h"
#include "itesteventsink.h"
#include "logentry.h"
#include "outputpaths.h"
#include "testcase81.h"
#include "testcaseregistry.h"
#include "testtypes.h"

#include <QDir>
#include <QElapsedTimer>
#include <QThread>
#include <QtTest>
#include <algorithm>
#include <memory>
#include <type_traits>

static_assert(std::has_virtual_destructor<IScpiTransport>::value,
              "IScpiTransport must be safely replaceable");
static_assert(std::has_virtual_destructor<ITestCase>::value,
              "ITestCase must be safely owned by the registry");

class Case81Capture : public ITestCaseView,
                      public ITestEventSink,
                      public ICase81ResultPresenter
{
public:
    void showCaseSummary(const QString &title,
                         const QString &frequency,
                         const QString &bandwidth,
                         int progress,
                         const QString &progressFormat) override
    {
        summary = QStringList({title, frequency, bandwidth,
                               QString::number(progress), progressFormat});
    }

    void addTestLog(const QString &level,
                    const QString &source,
                    const QString &message) override
    {
        logs.append(QStringList({level, source, message}));
    }

    void presentCase81(const Case81Result &value) override
    {
        result = value;
        ++presentCount;
    }

    QStringList summary;
    QList<QStringList> logs;
    Case81Result result;
    int presentCount = 0;
};

class AuditedFakeScpiTransport : public FakeScpiTransport
{
public:
    AuditedFakeScpiTransport(const QHash<QString, QByteArray> &responses,
                             const std::shared_ptr<QStringList> &audit,
                             int waitDelayMs = 0)
        : m_audit(audit)
    {
        for (auto it = responses.constBegin(); it != responses.constEnd(); ++it) {
            enqueueResponse(it.key(), it.value());
        }
        setWaitDelayMs(waitDelayMs);
    }

    qint64 write(const QByteArray &data) override
    {
        m_audit->append(QString::fromUtf8(data).trimmed());
        return FakeScpiTransport::write(data);
    }

private:
    std::shared_ptr<QStringList> m_audit;
};

class CleanupFailingScpiTransport : public AuditedFakeScpiTransport
{
public:
    CleanupFailingScpiTransport(const QHash<QString, QByteArray> &responses,
                                const std::shared_ptr<QStringList> &audit)
        : AuditedFakeScpiTransport(responses, audit)
    {
    }

    qint64 write(const QByteArray &data) override
    {
        const QString command = QString::fromUtf8(data).trimmed();
        if (command == ":ABORt"
            || command == ":INITiate:CONTinuous OFF"
            || command == ":OUTPut:ALL OFF"
            || command == ":OUTPut1:STATe OFF") {
            return -1;
        }
        return AuditedFakeScpiTransport::write(data);
    }
};

class AnalyzerSpectrumWriteFailingTransport : public AuditedFakeScpiTransport
{
public:
    AnalyzerSpectrumWriteFailingTransport(const QHash<QString, QByteArray> &responses,
                                          const std::shared_ptr<QStringList> &audit)
        : AuditedFakeScpiTransport(responses, audit)
    {
    }

    qint64 write(const QByteArray &data) override
    {
        const QString command = QString::fromUtf8(data).trimmed();
        if (command == ":CONFigure:SANalyzer") {
            return -1;
        }
        return AuditedFakeScpiTransport::write(data);
    }
};

class RetryCleanupScpiTransport : public AuditedFakeScpiTransport
{
public:
    RetryCleanupScpiTransport(const QHash<QString, QByteArray> &responses,
                              const std::shared_ptr<QStringList> &audit)
        : AuditedFakeScpiTransport(responses, audit)
    {
    }

    qint64 write(const QByteArray &data) override
    {
        const QString command = QString::fromUtf8(data).trimmed();
        if (command == ":ABORt"
            || command == ":INITiate:CONTinuous OFF"
            || command == ":OUTPut:ALL OFF"
            || command == ":OUTPut1:STATe OFF") {
            const int count = ++m_attemptCounts[command];
            if (count == 1) {
                return -1;
            }
        }
        return AuditedFakeScpiTransport::write(data);
    }

private:
    QHash<QString, int> m_attemptCounts;
};

class DisconnectTrackingCleanupTransport : public CleanupFailingScpiTransport
{
public:
    DisconnectTrackingCleanupTransport(const QHash<QString, QByteArray> &responses,
                                       const std::shared_ptr<QStringList> &audit,
                                       bool *disconnectCalled)
        : CleanupFailingScpiTransport(responses, audit)
        , m_disconnectCalled(disconnectCalled)
    {
    }

    void disconnectFromHost() override
    {
        if (m_disconnectCalled) {
            *m_disconnectCalled = true;
        }
        CleanupFailingScpiTransport::disconnectFromHost();
    }

private:
    bool *m_disconnectCalled;
};

class DelayedBinaryBlockTransport : public IScpiTransport
{
public:
    DelayedBinaryBlockTransport(const QHash<QString, QList<QByteArray>> &responses,
                                const std::shared_ptr<QStringList> &audit,
                                int binaryDelayMs)
        : m_connected(true)
        , m_responses(responses)
        , m_audit(audit)
        , m_binaryDelayMs(binaryDelayMs)
    {
    }

    void connectToHost(const QString &, quint16) override
    {
        m_connected = true;
        emit connected();
    }

    void disconnectFromHost() override
    {
        m_connected = false;
    }

    bool isConnected() const override
    {
        return m_connected;
    }

    bool waitForConnected(int) override
    {
        return m_connected;
    }

    qint64 write(const QByteArray &data) override
    {
        if (!m_connected) {
            return -1;
        }
        const QString command = QString::fromUtf8(data).trimmed();
        m_audit->append(command);
        m_commands.append(command);
        QList<QByteArray> &queued = m_responses[command];
        if (queued.isEmpty() && command.startsWith(":MMEMory:DATA?")) {
            queued = m_responses[QStringLiteral("__BINARY__")];
            m_responses[QStringLiteral("__BINARY__")].clear();
        }
        if (!queued.isEmpty()) {
            const QByteArray next = queued.takeFirst();
            if (command.startsWith(":MMEMory:DATA?")) {
                m_deferredBinary = next;
                m_binaryReady = false;
            } else {
                m_pendingData.append(next);
            }
        }
        return data.size();
    }

    bool flush() override
    {
        return true;
    }

    qint64 bytesAvailable() const override
    {
        return m_pendingData.size();
    }

    QByteArray readAll() override
    {
        const QByteArray value = m_pendingData;
        m_pendingData.clear();
        return value;
    }

    bool waitForReadyRead(int) override
    {
        if (!m_binaryReady && !m_deferredBinary.isEmpty()) {
            QThread::msleep(static_cast<unsigned long>(m_binaryDelayMs));
            if (!m_connected) {
                return false;
            }
            m_pendingData.append(m_deferredBinary);
            m_deferredBinary.clear();
            m_binaryReady = true;
        }
        return !m_pendingData.isEmpty();
    }

    bool waitForBytesWritten(int) override
    {
        return m_connected;
    }

    QString errorString() const override
    {
        return m_connected ? QString() : QStringLiteral("fake transport disconnected");
    }

    QStringList commandTrace() const
    {
        return m_commands;
    }

private:
    bool m_connected;
    mutable QByteArray m_pendingData;
    QHash<QString, QList<QByteArray>> m_responses;
    std::shared_ptr<QStringList> m_audit;
    QStringList m_commands;
    int m_binaryDelayMs = 0;
    QByteArray m_deferredBinary;
    bool m_binaryReady = false;
};

class SessionQueryThread : public QThread
{
public:
    SessionQueryThread(InstrumentSession *session,
                       const QString &command,
                       int timeoutMs,
                       const ScpiRequestOptions &options = ScpiRequestOptions())
        : m_session(session)
        , m_command(command)
        , m_timeoutMs(timeoutMs)
        , m_options(options)
    {
    }

    void run() override
    {
        reply = m_session->query(m_command, m_timeoutMs, m_options);
    }

    ScpiReply reply;

private:
    InstrumentSession *m_session;
    QString m_command;
    int m_timeoutMs;
    ScpiRequestOptions m_options;
};

class CoreUtilsTest : public QObject
{
    Q_OBJECT

private slots:
    void escapeCsvCell_data();
    void escapeCsvCell();
    void outputPaths();
    void testTypeNames();
    void valueEquality();
    void scpiSessionCase81Trace();
    void scpiSessionTimeout();
    void scpiSessionDisconnected();
    void scpiSessionBinaryBlock();
    void scpiSessionConcurrentQueries();
    void scpiSessionReconnectAfterDisconnect();
    void scpiSessionQueryAfterAbortActiveRequest();
    void scpiSessionSafetyAbort();
    void case81DualInstrument();
    void case81LegacyInstrument();
    void case81RegistryAndRepeat();
    void case81AsyncLifecycle();
    void case81AsyncCancellation();
    void case81AsyncCleanupFailure();
    void case81AsyncRepeat();
    void case82PowerPointParser();
    void case82AsyncLifecycle();
    void case82AsyncCancellation();
    void case82AsyncCleanupFailure();
    void case82AsyncCleanupForcedRetry();
    void case82AsyncCleanupDisconnectFailure();
    void case82AsyncSpectrumConfigFailure();
    void case82AsyncSpectrumErrorQueueFailure();
    void analyzerAcpowerParsing_data();
    void analyzerAcpowerParsing();
    void case85AsyncLifecycle();
    void case85AsyncCancellationDuringScreenshotDownload();
    void case85AsyncScreenshotLocalWriteFailure();
};

static QHash<QString, QByteArray> case82GeneratorResponses()
{
    return {{"*IDN?", "Ceyear,1466G-V,SN,1.0\n"},
            {":SOURce1:FREQuency?", "925000000\n"},
            {":SOURce1:POWer?", "0\n"},
            {":OUTPut1:STATe?", "1\n"},
            {":SYSTem:ERRor?", "+0,\"No error\"\n"}};
}

static AuditedFakeScpiTransport *newCase82AnalyzerTransport(
        const std::shared_ptr<QStringList> &audit,
        double targetPowerDbm,
        double frequencyMHz,
        int waitDelayMs = 0)
{
    auto *transport = new AuditedFakeScpiTransport(
        {{"*IDN?", "Ceyear,4071E,SN,1.0\n"},
         {"*OPC?", "1\n"},
         {":SYSTem:ERRor?", "+0,\"No error\"\n"}},
        audit,
        waitDelayMs);
    const QByteArray freq = QByteArray::number(frequencyMHz * 1000000.0, 'f', 0) + "\n";
    const QByteArray power = QByteArray::number(targetPowerDbm, 'f', 2) + "\n";
    for (int i = 0; i < Case82Constants::SampleCount; ++i) {
        transport->enqueueResponse(":CALCulate:MARKer1:X?", freq);
        transport->enqueueResponse(":CALCulate:MARKer1:Y?", power);
    }
    return transport;
}

static QHash<QString, QList<QByteArray>> case85AnalyzerResponses()
{
    return {
        {"*CLS", {}},
        {":SYSTem:ERRor?", {"+0,\"No error\"\n", "+0,\"No error\"\n", "+0,\"No error\"\n", "+0,\"No error\"\n"}},
        {"*OPC?", {"1\n"}},
        {":READ:ACPower?", {"20,-43.50,-44.10\n"}},
        {"__BINARY__", {"#2100123456789\n"}}
    };
}

void CoreUtilsTest::escapeCsvCell_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expected");

    QTest::newRow("plain") << QString("PASS") << QString("PASS");
    QTest::newRow("comma") << QString("925 MHz,200 kHz")
                            << QString("\"925 MHz,200 kHz\"");
    QTest::newRow("quote") << QString("value \"quoted\"")
                            << QString("\"value \"\"quoted\"\"\"");
    QTest::newRow("line-feed") << QString("line1\nline2")
                                << QString("\"line1\nline2\"");
    QTest::newRow("carriage-return") << QString("line1\rline2")
                                      << QString("\"line1\rline2\"");
    QTest::newRow("empty") << QString() << QString();
}

void CoreUtilsTest::escapeCsvCell()
{
    QFETCH(QString, input);
    QFETCH(QString, expected);

    QCOMPARE(CsvUtils::escapeCell(input), expected);
}

void CoreUtilsTest::outputPaths()
{
    const QDateTime dateTime(QDate(2026, 6, 23), QTime(17, 30, 45));

    QCOMPARE(OutputPaths::batchTimestamp(dateTime), QString("20260623_173045"));
    QCOMPARE(OutputPaths::fileTimestamp(dateTime), QString("173045"));
    QCOMPARE(OutputPaths::timestampedScreenshotName("point_1", dateTime),
             QString("point_1_173045.png"));
    QCOMPARE(OutputPaths::sanitizeFileToken("A +340,-53/BLER"),
             QString("A_p340_m53_BLER"));

    const QString expectedRunDirectory = QDir(OutputPaths::path("screenshots")).filePath(
        QDir("8.5_ACLR").filePath("20260623_173045"));
    QCOMPARE(OutputPaths::screenshotRunDirectory("8.5_ACLR", "20260623_173045"),
             expectedRunDirectory);
}

void CoreUtilsTest::testTypeNames()
{
    QCOMPARE(testCaseCode(TestCaseId::Case81), QString("8.1"));
    QCOMPARE(testCaseCode(TestCaseId::Case811), QString("8.11"));
    QCOMPARE(testStateName(TestState::WaitingForUserInput),
             QString("WaitingForUserInput"));
    QCOMPARE(completionReasonName(CompletionReason::ExecutionFailed),
             QString("ExecutionFailed"));
    QCOMPARE(testVerdictName(TestVerdict::Fail), QString("FAIL"));
}

void CoreUtilsTest::valueEquality()
{
    const TestCompletion first{
        CompletionReason::Completed,
        TestVerdict::Fail,
        "measurement outside limit"
    };
    const TestCompletion second = first;
    QVERIFY(first == second);

    const LogEntry log{
        QDateTime(QDate(2026, 6, 23), QTime(17, 30, 45)),
        "INFO",
        "8.1",
        "baseline"
    };
    const LogEntry copiedLog = log;
    QVERIFY(log == copiedLog);
}

void CoreUtilsTest::scpiSessionCase81Trace()
{
    auto *analyzerTransport = new FakeScpiTransport;
    analyzerTransport->setConnected(true);
    analyzerTransport->enqueueResponse("*IDN?", "Ceyear,4071E,SN,1.0\r\n");
    analyzerTransport->enqueueResponse(":SYSTem:ERRor?", "+0,\"No error\"\n");
    InstrumentSession analyzerSession(analyzerTransport);
    Analyzer4071 analyzer(&analyzerSession);

    auto *generatorTransport = new FakeScpiTransport;
    generatorTransport->setConnected(true);
    generatorTransport->enqueueResponse("*IDN?", "Ceyear,1466G-V,SN,1.0\n");
    generatorTransport->enqueueResponse(":SYSTem:ERRor?", "+0,\"No error\"\n");
    InstrumentSession generatorSession(generatorTransport);
    Generator1466 generator(&generatorSession);

    QCOMPARE(analyzer.identify().text, QString("Ceyear,4071E,SN,1.0"));
    QCOMPARE(generator.identify().text, QString("Ceyear,1466G-V,SN,1.0"));
    QCOMPARE(analyzer.readError().text, QString("+0,\"No error\""));
    QCOMPARE(generator.readError().text, QString("+0,\"No error\""));

    QCOMPARE(analyzerTransport->commandTrace(),
             QStringList({"*IDN?", ":SYSTem:ERRor?"}));
    QCOMPARE(generatorTransport->commandTrace(),
             QStringList({"*IDN?", ":SYSTem:ERRor?"}));
    QCOMPARE(analyzerTransport->writeTrace().first(), QByteArray("*IDN?\n"));
    QCOMPARE(generatorTransport->writeTrace().last(),
             QByteArray(":SYSTem:ERRor?\n"));
}

void CoreUtilsTest::scpiSessionTimeout()
{
    auto *transport = new FakeScpiTransport;
    transport->setConnected(true);
    InstrumentSession session(transport);

    const ScpiReply reply = session.query("*IDN?", 1);
    QCOMPARE(reply.status, ScpiStatus::Timeout);
    QCOMPARE(transport->commandTrace(), QStringList({"*IDN?"}));
    QVERIFY(!session.isQuerying());
}

void CoreUtilsTest::scpiSessionDisconnected()
{
    auto *transport = new FakeScpiTransport;
    InstrumentSession session(transport);

    QCOMPARE(session.send("*IDN?").status, ScpiStatus::NotConnected);
    QCOMPARE(session.query("*IDN?", 1).status, ScpiStatus::NotConnected);
    QVERIFY(transport->commandTrace().isEmpty());
}

void CoreUtilsTest::scpiSessionBinaryBlock()
{
    auto *transport = new FakeScpiTransport;
    transport->setConnected(true);
    transport->enqueueResponse(":MMEMory:DATA? \"screen.png\"",
                               "#2100123456789\n");
    InstrumentSession session(transport);

    const BinaryBlockReply reply =
        session.queryBinaryBlock(":MMEMory:DATA? \"screen.png\"", 10);
    QCOMPARE(reply.status, ScpiStatus::Success);
    QCOMPARE(reply.payload, QByteArray("0123456789"));
}

void CoreUtilsTest::scpiSessionConcurrentQueries()
{
    auto *transport = new FakeScpiTransport;
    transport->setConnected(true);
    transport->setWaitDelayMs(20);
    transport->enqueueResponse("Q1?", "ONE\n");
    transport->enqueueResponse("Q2?", "TWO\n");
    InstrumentSession session(transport);

    SessionQueryThread first(&session, "Q1?", 500);
    SessionQueryThread second(&session, "Q2?", 500);
    first.start();
    second.start();

    QVERIFY(first.wait(2000));
    QVERIFY(second.wait(2000));
    QCOMPARE(first.reply.status, ScpiStatus::Success);
    QCOMPARE(second.reply.status, ScpiStatus::Success);
    QCOMPARE(first.reply.text, QString("ONE"));
    QCOMPARE(second.reply.text, QString("TWO"));
    QCOMPARE(transport->commandTrace().size(), 2);
    QVERIFY(transport->commandTrace().contains(QStringLiteral("Q1?")));
    QVERIFY(transport->commandTrace().contains(QStringLiteral("Q2?")));
}

void CoreUtilsTest::scpiSessionReconnectAfterDisconnect()
{
    auto *transport = new FakeScpiTransport;
    InstrumentSession session(transport);

    session.connectToHost("analyzer", 5025);
    session.disconnectFromHost();
    transport->enqueueResponse("*IDN?", "Ceyear,4071E,SN,1.0\n");
    session.connectToHost("analyzer", 5025);

    const ScpiReply reply = session.query("*IDN?", 500);
    QCOMPARE(reply.status, ScpiStatus::Success);
    QCOMPARE(reply.text, QString("Ceyear,4071E,SN,1.0"));
}

void CoreUtilsTest::scpiSessionQueryAfterAbortActiveRequest()
{
    auto *transport = new FakeScpiTransport;
    transport->setConnected(true);
    transport->setWaitDelayMs(20);
    InstrumentSession session(transport);

    SessionQueryThread hangingQuery(&session, "HANG?", 1000);
    hangingQuery.start();
    QTRY_VERIFY(transport->commandTrace().contains("HANG?"));

    QVERIFY(session.abortActiveRequest());
    QVERIFY(hangingQuery.wait(2000));
    QCOMPARE(hangingQuery.reply.status, ScpiStatus::Cancelled);

    transport->enqueueResponse("*IDN?", "Ceyear,4071E,SN,1.0\n");
    const ScpiReply reply = session.query("*IDN?", 500);
    QCOMPARE(reply.status, ScpiStatus::Success);
    QCOMPARE(reply.text, QString("Ceyear,4071E,SN,1.0"));
}

void CoreUtilsTest::scpiSessionSafetyAbort()
{
    auto *transport = new FakeScpiTransport;
    transport->setConnected(true);
    transport->setWaitDelayMs(20);
    InstrumentSession session(transport);

    SessionQueryThread queryThread(&session, "HANG?", 1000);
    queryThread.start();
    QTRY_VERIFY(transport->commandTrace().contains("HANG?"));

    ScpiRequestOptions cleanupOptions;
    cleanupOptions.priority = ScpiRequestPriority::Safety;
    cleanupOptions.abortActiveRequest = true;
    const ScpiWriteResult cleanupResult =
        session.send(":OUTPut:ALL OFF", cleanupOptions);

    QVERIFY(queryThread.wait(2000));
    QCOMPARE(queryThread.reply.status, ScpiStatus::Cancelled);
    QCOMPARE(cleanupResult.status, ScpiStatus::Success);
    QCOMPARE(transport->commandTrace(), QStringList({"HANG?", ":OUTPut:ALL OFF"}));
}

void CoreUtilsTest::case81DualInstrument()
{
    auto *analyzerTransport = new FakeScpiTransport;
    analyzerTransport->setConnected(true);
    analyzerTransport->enqueueResponse("*IDN?", "Ceyear,4071E,SN,1.0\n");
    analyzerTransport->enqueueResponse(":SYSTem:ERRor?", "+0,\"No error\"\n");
    InstrumentSession analyzerSession(analyzerTransport);
    Analyzer4071 analyzer(&analyzerSession);

    auto *generatorTransport = new FakeScpiTransport;
    generatorTransport->setConnected(true);
    generatorTransport->enqueueResponse("*IDN?", "Ceyear,1466G-V,SN,1.0\n");
    generatorTransport->enqueueResponse(":SYSTem:ERRor?", "+0,\"No error\"\n");
    InstrumentSession generatorSession(generatorTransport);
    Generator1466 generator(&generatorSession);
    Case81Capture capture;
    TestCase81 testCase(&analyzer, &generator, &capture, &capture, &capture);

    const TestCompletion completion = testCase.start();
    QCOMPARE(completion.reason, CompletionReason::Completed);
    QCOMPARE(completion.verdict, TestVerdict::Pass);
    QCOMPARE(capture.presentCount, 1);
    QVERIFY(capture.result.dualInstrumentMode);
    QVERIFY(capture.result.overallPass);
    QCOMPARE(capture.result.analyzerIdn, QString("Ceyear,4071E,SN,1.0"));
    QCOMPARE(capture.result.generatorIdn, QString("Ceyear,1466G-V,SN,1.0"));
    QCOMPARE(capture.summary,
             QStringList({"8.1 基本功能验证", "---", "---", "100",
                          "8.1 基本功能 %p%"}));
    QCOMPARE(analyzerTransport->commandTrace(),
             QStringList({"*IDN?", ":SYSTem:ERRor?"}));
    QCOMPARE(generatorTransport->commandTrace(),
             QStringList({"*IDN?", ":SYSTem:ERRor?"}));
    QCOMPARE(capture.logs.last(),
             QStringList({"PASS", "测试", "8.1 双仪表基本通信验证完成"}));
}

void CoreUtilsTest::case81LegacyInstrument()
{
    auto *analyzerTransport = new FakeScpiTransport;
    analyzerTransport->setConnected(true);
    analyzerTransport->enqueueResponse("*IDN?", "Ceyear,4071E,SN,1.0\n");
    analyzerTransport->enqueueResponse("MEAS:VOLT:DC?", "1.25\n");
    InstrumentSession analyzerSession(analyzerTransport);
    Analyzer4071 analyzer(&analyzerSession);

    auto *generatorTransport = new FakeScpiTransport;
    InstrumentSession generatorSession(generatorTransport);
    Generator1466 generator(&generatorSession);
    Case81Capture capture;
    TestCase81 testCase(&analyzer, &generator, &capture, &capture, &capture);

    const TestCompletion completion = testCase.start();
    QCOMPARE(completion.verdict, TestVerdict::Pass);
    QVERIFY(!capture.result.dualInstrumentMode);
    QVERIFY(capture.result.voltageAvailable);
    QCOMPARE(capture.result.voltage, 1.25);
    QCOMPARE(analyzerTransport->commandTrace(),
             QStringList({"*IDN?", "MEAS:VOLT:DC?"}));
    QCOMPARE(capture.logs.last(),
             QStringList({"PASS", "测试", "8.1 基本功能验证完成"}));
}

void CoreUtilsTest::case81RegistryAndRepeat()
{
    auto *analyzerTransport = new FakeScpiTransport;
    analyzerTransport->setConnected(true);
    InstrumentSession analyzerSession(analyzerTransport);
    Analyzer4071 analyzer(&analyzerSession);
    auto *generatorTransport = new FakeScpiTransport;
    generatorTransport->setConnected(true);
    InstrumentSession generatorSession(generatorTransport);
    Generator1466 generator(&generatorSession);
    Case81Capture capture;

    TestCaseRegistry registry;
    registry.registerCase(std::unique_ptr<ITestCase>(
        new TestCase81(&analyzer, &generator, &capture, &capture, &capture)));
    ITestCase *testCase = registry.find(TestCaseId::Case81);
    QVERIFY(testCase);
    QCOMPARE(testCase->id(), TestCaseId::Case81);

    for (int run = 0; run < 20; ++run) {
        analyzerTransport->enqueueResponse("*IDN?", "4071E\n");
        analyzerTransport->enqueueResponse(":SYSTem:ERRor?", "+0,\"No error\"\n");
        generatorTransport->enqueueResponse("*IDN?", "1466G-V\n");
        generatorTransport->enqueueResponse(":SYSTem:ERRor?", "+0,\"No error\"\n");
        QCOMPARE(testCase->start().verdict, TestVerdict::Pass);
        testCase->requestStop();
        testCase->requestStop();
    }
    QCOMPARE(capture.presentCount, 20);
}

void CoreUtilsTest::case81AsyncLifecycle()
{
    auto analyzerAudit = std::make_shared<QStringList>();
    auto generatorAudit = std::make_shared<QStringList>();
    Case81RunController controller;
    controller.setTransportFactories(
        [analyzerAudit]() {
            return new AuditedFakeScpiTransport(
                {{"*IDN?", "Ceyear,4071E,SN,1.0\n"},
                 {":SYSTem:ERRor?", "+0,\"No error\"\n"}},
                analyzerAudit);
        },
        [generatorAudit]() {
            return new AuditedFakeScpiTransport(
                {{"*IDN?", "Ceyear,1466G-V,SN,1.0\n"},
                 {":SYSTem:ERRor?", "+0,\"No error\"\n"}},
                generatorAudit);
        });

    QList<TestState> states;
    QStringList eventOrder;
    int finishCount = 0;
    TestCompletion completion;
    connect(&controller, &Case81RunController::stateChanged,
            this, [&states](TestState state) { states.append(state); });
    connect(&controller, &Case81RunController::cleanupCompleted,
            this, [&eventOrder](bool safe) {
        QVERIFY(safe);
        eventOrder.append("cleanup");
    });
    connect(&controller, &Case81RunController::finished,
            this, [&eventOrder, &finishCount, &completion](const TestCompletion &value) {
        eventOrder.append("finished");
        ++finishCount;
        completion = value;
    });

    Case81RunConfig config{"analyzer", 5025, "generator", 5025};
    QElapsedTimer startTimer;
    startTimer.start();
    QVERIFY(controller.start(config));
    QVERIFY(startTimer.elapsed() < 100);
    QTRY_COMPARE(finishCount, 1);
    QCOMPARE(completion.reason, CompletionReason::Completed);
    QCOMPARE(completion.verdict, TestVerdict::Pass);
    QCOMPARE(eventOrder, QStringList({"cleanup", "finished"}));
    QCOMPARE(states,
             QList<TestState>({TestState::Validating,
                               TestState::Preparing,
                               TestState::Running,
                               TestState::CleaningUp,
                               TestState::Completed}));
    QCOMPARE(*analyzerAudit,
             QStringList({"*IDN?", ":SYSTem:ERRor?",
                          ":ABORt", ":INITiate:CONTinuous OFF"}));
    QCOMPARE(*generatorAudit,
             QStringList({"*IDN?", ":SYSTem:ERRor?",
                          ":OUTPut:ALL OFF", ":OUTPut1:STATe OFF"}));
}

void CoreUtilsTest::case81AsyncCancellation()
{
    auto analyzerAudit = std::make_shared<QStringList>();
    Case81RunController controller;
    controller.setTransportFactories(
        [analyzerAudit]() {
            return new AuditedFakeScpiTransport({}, analyzerAudit, 50);
        },
        []() {
            return new AuditedFakeScpiTransport(
                {}, std::make_shared<QStringList>());
        });

    QStringList eventOrder;
    int finishCount = 0;
    TestCompletion completion;
    connect(&controller, &Case81RunController::cleanupCompleted,
            this, [&eventOrder](bool) { eventOrder.append("cleanup"); });
    connect(&controller, &Case81RunController::finished,
            this, [&eventOrder, &finishCount, &completion](const TestCompletion &value) {
        eventOrder.append("finished");
        ++finishCount;
        completion = value;
    });

    Case81RunConfig config{"analyzer", 5025, "generator", 5025};
    QVERIFY(controller.start(config));
    QTRY_COMPARE(controller.state(), TestState::Running);

    QElapsedTimer stopTimer;
    stopTimer.start();
    controller.requestStop();
    QCOMPARE(controller.state(), TestState::Stopping);
    QVERIFY(stopTimer.elapsed() < 500);
    controller.requestStop();
    controller.requestStop();

    QTRY_COMPARE(finishCount, 1);
    QCOMPARE(completion.reason, CompletionReason::Cancelled);
    QCOMPARE(eventOrder, QStringList({"cleanup", "finished"}));
    QCOMPARE(controller.state(), TestState::Cancelled);
    QVERIFY(analyzerAudit->contains(":ABORt"));
    QVERIFY(analyzerAudit->contains(":INITiate:CONTinuous OFF"));
    QTest::qWait(50);
    QCOMPARE(finishCount, 1);
}

void CoreUtilsTest::case81AsyncCleanupFailure()
{
    Case81RunController controller;
    controller.setTransportFactories(
        []() {
            return new CleanupFailingScpiTransport(
                {{"*IDN?", "4071E\n"},
                 {":SYSTem:ERRor?", "+0,\"No error\"\n"}},
                std::make_shared<QStringList>());
        },
        []() {
            return new AuditedFakeScpiTransport(
                {{"*IDN?", "1466G-V\n"},
                 {":SYSTem:ERRor?", "+0,\"No error\"\n"}},
                std::make_shared<QStringList>());
        });

    QStringList eventOrder;
    bool cleanupSafe = true;
    int finishCount = 0;
    TestCompletion completion;
    connect(&controller, &Case81RunController::cleanupCompleted,
            this, [&eventOrder, &cleanupSafe](bool safe) {
        cleanupSafe = safe;
        eventOrder.append("cleanup");
    });
    connect(&controller, &Case81RunController::finished,
            this, [&eventOrder, &finishCount, &completion](const TestCompletion &value) {
        eventOrder.append("finished");
        ++finishCount;
        completion = value;
    });

    QVERIFY(controller.start({"analyzer", 5025, "generator", 5025}));
    QTRY_COMPARE(finishCount, 1);
    QVERIFY(!cleanupSafe);
    QCOMPARE(eventOrder, QStringList({"cleanup", "finished"}));
    QCOMPARE(completion.reason, CompletionReason::ExecutionFailed);
    QCOMPARE(completion.detail, QString("instrument safety cleanup failed"));
    QCOMPARE(controller.state(), TestState::ExecutionFailed);
}

void CoreUtilsTest::case81AsyncRepeat()
{
    Case81RunController controller;
    controller.setTransportFactories(
        []() {
            return new AuditedFakeScpiTransport(
                {{"*IDN?", "4071E\n"},
                 {":SYSTem:ERRor?", "+0,\"No error\"\n"}},
                std::make_shared<QStringList>());
        },
        []() {
            return new AuditedFakeScpiTransport(
                {{"*IDN?", "1466G-V\n"},
                 {":SYSTem:ERRor?", "+0,\"No error\"\n"}},
                std::make_shared<QStringList>());
        });

    int finishCount = 0;
    connect(&controller, &Case81RunController::finished,
            this, [&finishCount](const TestCompletion &) { ++finishCount; });
    const Case81RunConfig config{"analyzer", 5025, "generator", 5025};
    for (int run = 0; run < 20; ++run) {
        QVERIFY(controller.start(config));
        QTRY_COMPARE(finishCount, run + 1);
        QCOMPARE(controller.state(), TestState::Completed);
    }
}

void CoreUtilsTest::case82PowerPointParser()
{
    bool ok = false;
    QString error;
    QCOMPARE(parseCase82PowerPoints("", &ok, &error),
             QList<double>({0.0, 3.0, 7.5, 12.0, 15.0}));
    QVERIFY(ok);

    QCOMPARE(parseCase82PowerPoints("0, 3 3；7.5，12、15", &ok, &error),
             QList<double>({0.0, 3.0, 7.5, 12.0, 15.0}));
    QVERIFY(ok);

    parseCase82PowerPoints("16", &ok, &error);
    QVERIFY(!ok);
    QCOMPARE(error, QString("8.2 功率点 16.00 dBm 超出当前允许范围 0~15 dBm"));

    parseCase82PowerPoints("abc", &ok, &error);
    QVERIFY(!ok);
    QCOMPARE(error, QString("8.2 功率点包含非法数值: abc"));
}

void CoreUtilsTest::case82AsyncLifecycle()
{
    auto analyzerAudit = std::make_shared<QStringList>();
    auto generatorAudit = std::make_shared<QStringList>();
    Case82RunController controller;
    controller.setTransportFactories(
        [analyzerAudit]() {
            return newCase82AnalyzerTransport(analyzerAudit, 0.0, 925.0);
        },
        [generatorAudit]() {
            return new AuditedFakeScpiTransport(
                case82GeneratorResponses(),
                generatorAudit);
        });

    QList<TestState> states;
    QStringList eventOrder;
    int finishCount = 0;
    int rowCount = 0;
    int sampleCount = 0;
    TestCompletion completion;
    Case82Result result;
    connect(&controller, &Case82RunController::stateChanged,
            this, [&states](TestState state) { states.append(state); });
    connect(&controller, &Case82RunController::sampleReady,
            this, [&sampleCount](const Case82Sample &) { ++sampleCount; });
    connect(&controller, &Case82RunController::rowReady,
            this, [&rowCount](int, const Case82RowResult &) { ++rowCount; });
    connect(&controller, &Case82RunController::resultReady,
            this, [&result](const Case82Result &value) { result = value; });
    connect(&controller, &Case82RunController::cleanupCompleted,
            this, [&eventOrder](bool safe) {
        QVERIFY(safe);
        eventOrder.append("cleanup");
    });
    connect(&controller, &Case82RunController::finished,
            this, [&eventOrder, &finishCount, &completion](const TestCompletion &value) {
        eventOrder.append("finished");
        ++finishCount;
        completion = value;
    });

    Case82RunConfig config;
    config.analyzerHost = "analyzer";
    config.generatorHost = "generator";
    config.powerPoints = {0.0};
    config.frequencyMHz = 925.0;
    config.bandwidth = "200";
    config.spanMHz = 1.0;

    QElapsedTimer startTimer;
    startTimer.start();
    QVERIFY(controller.start(config));
    QVERIFY(startTimer.elapsed() < 100);
    QTRY_COMPARE(finishCount, 1);
    QCOMPARE(completion.reason, CompletionReason::Completed);
    QCOMPARE(completion.verdict, TestVerdict::Pass);
    QCOMPARE(eventOrder, QStringList({"cleanup", "finished"}));
    QCOMPARE(states,
             QList<TestState>({TestState::Validating,
                               TestState::Preparing,
                               TestState::Running,
                               TestState::CleaningUp,
                               TestState::Completed}));
    QCOMPARE(rowCount, 1);
    QCOMPARE(sampleCount, Case82Constants::SampleCount);
    QCOMPARE(result.rows.size(), 1);
    QVERIFY(result.overallPass);
    QCOMPARE(*generatorAudit,
             QStringList({"*IDN?", "*CLS",
                          ":SOURce1:FREQuency 925.000000MHz",
                          ":SOURce1:POWer 0.00dBm",
                          ":OUTPut1:STATe ON",
                          ":SOURce1:FREQuency?",
                          ":SOURce1:POWer?",
                          ":OUTPut1:STATe?",
                          ":SYSTem:ERRor?",
                          ":OUTPut:ALL OFF",
                          ":OUTPut1:STATe OFF"}));
    QVERIFY(analyzerAudit->mid(0, 10).contains(":CONFigure:SANalyzer"));
    QCOMPARE(analyzerAudit->count(":CALCulate:MARKer1:MAXimum"),
             Case82Constants::SampleCount);
    QCOMPARE(analyzerAudit->mid(analyzerAudit->size() - 2),
             QStringList({":ABORt", ":INITiate:CONTinuous OFF"}));
}

void CoreUtilsTest::case82AsyncCancellation()
{
    auto analyzerAudit = std::make_shared<QStringList>();
    auto generatorAudit = std::make_shared<QStringList>();
    Case82RunController controller;
    controller.setTransportFactories(
        [analyzerAudit]() {
            return newCase82AnalyzerTransport(analyzerAudit, 0.0, 925.0, 50);
        },
        [generatorAudit]() {
            return new AuditedFakeScpiTransport(
                case82GeneratorResponses(),
                generatorAudit,
                50);
        });

    QStringList eventOrder;
    int finishCount = 0;
    TestCompletion completion;
    connect(&controller, &Case82RunController::cleanupCompleted,
            this, [&eventOrder](bool) { eventOrder.append("cleanup"); });
    connect(&controller, &Case82RunController::finished,
            this, [&eventOrder, &finishCount, &completion](const TestCompletion &value) {
        eventOrder.append("finished");
        ++finishCount;
        completion = value;
    });

    Case82RunConfig config;
    config.analyzerHost = "analyzer";
    config.generatorHost = "generator";
    config.powerPoints = {0.0, 3.0};
    config.frequencyMHz = 925.0;
    config.bandwidth = "200";
    config.spanMHz = 1.0;
    QVERIFY(controller.start(config));
    QTRY_COMPARE(controller.state(), TestState::Running);

    QElapsedTimer stopTimer;
    stopTimer.start();
    controller.requestStop();
    QCOMPARE(controller.state(), TestState::Stopping);
    QVERIFY(stopTimer.elapsed() < 500);
    controller.requestStop();
    controller.requestStop();

    QTRY_COMPARE(finishCount, 1);
    QCOMPARE(completion.reason, CompletionReason::Cancelled);
    QCOMPARE(eventOrder, QStringList({"cleanup", "finished"}));
    QCOMPARE(controller.state(), TestState::Cancelled);
    QVERIFY(analyzerAudit->contains(":ABORt"));
    QVERIFY(generatorAudit->contains(":OUTPut:ALL OFF"));
    QTest::qWait(50);
    QCOMPARE(finishCount, 1);
}

void CoreUtilsTest::case82AsyncCleanupFailure()
{
    Case82RunController controller;
    controller.setTransportFactories(
        []() {
            auto *transport = new CleanupFailingScpiTransport(
                {{"*IDN?", "Ceyear,4071E,SN,1.0\n"},
                 {"*OPC?", "1\n"},
                 {":SYSTem:ERRor?", "+0,\"No error\"\n"}},
                std::make_shared<QStringList>());
            for (int i = 0; i < Case82Constants::SampleCount; ++i) {
                transport->enqueueResponse(":CALCulate:MARKer1:X?", "925000000\n");
                transport->enqueueResponse(":CALCulate:MARKer1:Y?", "0\n");
            }
            return transport;
        },
        []() {
            return new AuditedFakeScpiTransport(
                case82GeneratorResponses(),
                std::make_shared<QStringList>());
        });

    QStringList eventOrder;
    bool cleanupSafe = true;
    int finishCount = 0;
    TestCompletion completion;
    connect(&controller, &Case82RunController::cleanupCompleted,
            this, [&eventOrder, &cleanupSafe](bool safe) {
        cleanupSafe = safe;
        eventOrder.append("cleanup");
    });
    connect(&controller, &Case82RunController::finished,
            this, [&eventOrder, &finishCount, &completion](const TestCompletion &value) {
        eventOrder.append("finished");
        ++finishCount;
        completion = value;
    });

    Case82RunConfig config;
    config.analyzerHost = "analyzer";
    config.generatorHost = "generator";
    config.powerPoints = {0.0};
    config.frequencyMHz = 925.0;
    config.bandwidth = "200";
    config.spanMHz = 1.0;
    QVERIFY(controller.start(config));
    QTRY_COMPARE(finishCount, 1);
    QVERIFY(!cleanupSafe);
    QCOMPARE(eventOrder, QStringList({"cleanup", "finished"}));
    QCOMPARE(completion.reason, CompletionReason::ExecutionFailed);
    QCOMPARE(completion.detail, QString("instrument safety cleanup failed"));
    QCOMPARE(controller.state(), TestState::ExecutionFailed);
}

void CoreUtilsTest::case82AsyncCleanupForcedRetry()
{
    auto analyzerAudit = std::make_shared<QStringList>();
    auto generatorAudit = std::make_shared<QStringList>();
    Case82RunController controller;
    controller.setTransportFactories(
        [analyzerAudit]() {
            auto *transport = new RetryCleanupScpiTransport(
                {{"*IDN?", "Ceyear,4071E,SN,1.0\n"},
                 {"*OPC?", "1\n"},
                 {":SYSTem:ERRor?", "+0,\"No error\"\n"}},
                analyzerAudit);
            for (int i = 0; i < Case82Constants::SampleCount; ++i) {
                transport->enqueueResponse(":CALCulate:MARKer1:X?", "925000000\n");
                transport->enqueueResponse(":CALCulate:MARKer1:Y?", "0\n");
            }
            return transport;
        },
        [generatorAudit]() {
            return new RetryCleanupScpiTransport(
                case82GeneratorResponses(),
                generatorAudit);
        });

    QStringList logs;
    int finishCount = 0;
    bool cleanupSafe = false;
    TestCompletion completion;
    connect(&controller, &Case82RunController::logReady,
            this, [&logs](const QString &, const QString &, const QString &message) {
        logs.append(message);
    });
    connect(&controller, &Case82RunController::cleanupCompleted,
            this, [&cleanupSafe](bool safe) { cleanupSafe = safe; });
    connect(&controller, &Case82RunController::finished,
            this, [&finishCount, &completion](const TestCompletion &value) {
        ++finishCount;
        completion = value;
    });

    Case82RunConfig config;
    config.analyzerHost = "analyzer";
    config.generatorHost = "generator";
    config.powerPoints = {0.0};
    config.frequencyMHz = 925.0;
    config.bandwidth = "200";
    config.spanMHz = 1.0;
    QVERIFY(controller.start(config));
    QTRY_COMPARE(finishCount, 1);

    QVERIFY(cleanupSafe);
    QCOMPARE(completion.reason, CompletionReason::Completed);
    QVERIFY(analyzerAudit->contains(QStringLiteral(":ABORt"))
            || analyzerAudit->contains(QStringLiteral(":INITiate:CONTinuous OFF")));
    QVERIFY(generatorAudit->contains(QStringLiteral(":OUTPut:ALL OFF"))
            || generatorAudit->contains(QStringLiteral(":OUTPut1:STATe OFF")));
    QVERIFY(std::any_of(logs.cbegin(), logs.cend(), [](const QString &message) {
        return message.contains(QStringLiteral("常规安全清理失败"));
    }));
}

void CoreUtilsTest::case82AsyncCleanupDisconnectFailure()
{
    auto analyzerAudit = std::make_shared<QStringList>();
    bool analyzerDisconnected = false;
    Case82RunController controller;
    controller.setTransportFactories(
        [analyzerAudit, &analyzerDisconnected]() {
            auto *transport = new DisconnectTrackingCleanupTransport(
                {{"*IDN?", "Ceyear,4071E,SN,1.0\n"},
                 {"*OPC?", "1\n"},
                 {":SYSTem:ERRor?", "+0,\"No error\"\n"}},
                analyzerAudit,
                &analyzerDisconnected);
            for (int i = 0; i < Case82Constants::SampleCount; ++i) {
                transport->enqueueResponse(":CALCulate:MARKer1:X?", "925000000\n");
                transport->enqueueResponse(":CALCulate:MARKer1:Y?", "0\n");
            }
            return transport;
        },
        []() {
            return new AuditedFakeScpiTransport(
                case82GeneratorResponses(),
                std::make_shared<QStringList>());
        });

    QStringList logs;
    int finishCount = 0;
    bool cleanupSafe = true;
    TestCompletion completion;
    connect(&controller, &Case82RunController::logReady,
            this, [&logs](const QString &level, const QString &, const QString &message) {
        logs.append(level + "|" + message);
    });
    connect(&controller, &Case82RunController::cleanupCompleted,
            this, [&cleanupSafe](bool safe) { cleanupSafe = safe; });
    connect(&controller, &Case82RunController::finished,
            this, [&finishCount, &completion](const TestCompletion &value) {
        ++finishCount;
        completion = value;
    });

    Case82RunConfig config;
    config.analyzerHost = "analyzer";
    config.generatorHost = "generator";
    config.powerPoints = {0.0};
    config.frequencyMHz = 925.0;
    config.bandwidth = "200";
    config.spanMHz = 1.0;
    QVERIFY(controller.start(config));
    QTRY_COMPARE(finishCount, 1);

    QVERIFY(!cleanupSafe);
    QVERIFY(analyzerDisconnected);
    QCOMPARE(completion.reason, CompletionReason::ExecutionFailed);
    QVERIFY(std::any_of(logs.cbegin(), logs.cend(), [](const QString &entry) {
        return entry.contains(QStringLiteral("ERROR|"))
                && entry.contains(QStringLiteral("请人工确认仪表输出状态"));
    }));
}

void CoreUtilsTest::case82AsyncSpectrumConfigFailure()
{
    auto analyzerAudit = std::make_shared<QStringList>();
    Case82RunController controller;
    controller.setTransportFactories(
        [analyzerAudit]() {
            return new AnalyzerSpectrumWriteFailingTransport(
                {{"*IDN?", "Ceyear,4071E,SN,1.0\n"},
                 {"*OPC?", "1\n"},
                 {":SYSTem:ERRor?", "+0,\"No error\"\n"}},
                analyzerAudit);
        },
        []() {
            return new AuditedFakeScpiTransport(
                case82GeneratorResponses(),
                std::make_shared<QStringList>());
        });

    QStringList eventOrder;
    int finishCount = 0;
    Case82Result result;
    TestCompletion completion;
    connect(&controller, &Case82RunController::resultReady,
            this, [&result](const Case82Result &value) { result = value; });
    connect(&controller, &Case82RunController::cleanupCompleted,
            this, [&eventOrder](bool) { eventOrder.append("cleanup"); });
    connect(&controller, &Case82RunController::finished,
            this, [&eventOrder, &finishCount, &completion](const TestCompletion &value) {
        eventOrder.append("finished");
        ++finishCount;
        completion = value;
    });

    Case82RunConfig config;
    config.analyzerHost = "analyzer";
    config.generatorHost = "generator";
    config.powerPoints = {0.0};
    config.frequencyMHz = 925.0;
    config.bandwidth = "200";
    config.spanMHz = 1.0;
    QVERIFY(controller.start(config));
    QTRY_COMPARE(finishCount, 1);

    QCOMPARE(eventOrder, QStringList({"cleanup", "finished"}));
    QCOMPARE(completion.reason, CompletionReason::Completed);
    QCOMPARE(completion.verdict, TestVerdict::Fail);
    QCOMPARE(result.rows.size(), 1);
    QVERIFY(!result.rows.first().pass);
    QCOMPARE(result.rows.first().failureReason, QString("4071 配置失败"));
    QCOMPARE(result.rows.first().sampleCount, 0);
    QCOMPARE(analyzerAudit->count(":CALCulate:MARKer1:MAXimum"), 0);
    QCOMPARE(controller.state(), TestState::Completed);
}

void CoreUtilsTest::analyzerAcpowerParsing_data()
{
    QTest::addColumn<QString>("response");
    QTest::addColumn<double>("expectedMain");
    QTest::addColumn<double>("expectedLeftAclr");
    QTest::addColumn<double>("expectedRightAclr");
    QTest::addColumn<QString>("detailsFragment");

    QTest::newRow("extended-six-field")
        << QString("20.00,20.00,-43.50,-23.50,-44.10,-24.10\n")
        << 20.00 << 43.50 << 44.10 << QString("extended ACP tuple");
    QTest::newRow("legacy-power-list")
        << QString("20.00,-23.50,-24.10,0,0\n")
        << 20.00 << 43.50 << 44.10 << QString("legacy power list");
    QTest::newRow("relative-aclr-tuple")
        << QString("20.00,-43.50,-44.10\n")
        << 20.00 << 43.50 << 44.10 << QString("relative ACLR tuple");
    QTest::newRow("direct-aclr-tuple")
        << QString("20.00,43.50,44.10\n")
        << 20.00 << 43.50 << 44.10 << QString("direct ACLR tuple");
}

void CoreUtilsTest::analyzerAcpowerParsing()
{
    QFETCH(QString, response);
    QFETCH(double, expectedMain);
    QFETCH(double, expectedLeftAclr);
    QFETCH(double, expectedRightAclr);
    QFETCH(QString, detailsFragment);

    auto audit = std::make_shared<QStringList>();
    QHash<QString, QByteArray> responses;
    responses.insert("*OPC?", "1\n");
    responses.insert(":READ:ACPower?", response.toUtf8());
    auto *transport = new AuditedFakeScpiTransport(responses, audit);
    InstrumentSession session(transport);
    Analyzer4071 analyzer(&session);
    session.connectToHost("analyzer", 5025);

    const Analyzer4071AclrMeasurementResult result = analyzer.measureAclr();
    QVERIFY(result.ok);
    QCOMPARE(result.mainPowerDbm, expectedMain);
    QCOMPARE(result.leftAclrDb, expectedLeftAclr);
    QCOMPARE(result.rightAclrDb, expectedRightAclr);
    QVERIFY(result.details.contains(detailsFragment));
}

void CoreUtilsTest::case85AsyncLifecycle()
{
    auto analyzerAudit = std::make_shared<QStringList>();
    auto generatorAudit = std::make_shared<QStringList>();
    Case85RunController controller;
    controller.setTransportFactories(
        [analyzerAudit]() {
            QHash<QString, QByteArray> flattened;
            flattened.insert("*OPC?", "1\n");
            flattened.insert(":READ:ACPower?", "20,-43.50,-44.10\n");
            flattened.insert(":SYSTem:ERRor?", "+0,\"No error\"\n");
            auto *transport = new AuditedFakeScpiTransport(flattened, analyzerAudit);
            transport->enqueueResponse(":SYSTem:ERRor?", "+0,\"No error\"\n");
            transport->enqueueResponse(":SYSTem:ERRor?", "+0,\"No error\"\n");
            transport->enqueueResponse(":SYSTem:ERRor?", "+0,\"No error\"\n");
            transport->enqueueResponse(":MMEMory:DATA? \"unused\"", "#2100123456789\n");
            return transport;
        },
        [generatorAudit]() {
            auto *transport = new AuditedFakeScpiTransport({}, generatorAudit);
            transport->enqueueResponse(":SOURce1:RADio:ARB:SCLock:RATE?", "1920000\n");
            transport->enqueueResponse(":SYSTem:ERRor?", "+0,\"No error\"\n");
            return transport;
        });

    QList<TestState> states;
    QStringList eventOrder;
    int finishCount = 0;
    int rowCount = 0;
    TestCompletion completion;
    Case85Result result;
    connect(&controller, &Case85RunController::stateChanged,
            this, [&states](TestState state) { states.append(state); });
    connect(&controller, &Case85RunController::rowReady,
            this, [&rowCount](int, const Case85RowResult &) { ++rowCount; });
    connect(&controller, &Case85RunController::resultReady,
            this, [&result](const Case85Result &value) { result = value; });
    connect(&controller, &Case85RunController::cleanupCompleted,
            this, [&eventOrder](bool safe) {
        QVERIFY(safe);
        eventOrder.append("cleanup");
    });
    connect(&controller, &Case85RunController::finished,
            this, [&eventOrder, &finishCount, &completion](const TestCompletion &value) {
        eventOrder.append("finished");
        ++finishCount;
        completion = value;
    });

    Case85RunConfig config;
    config.realInstrumentMode = false;
    config.txPowerDbm = 0.0;
    config.frequencyTextByBandwidth.insert(200, "925");
    config.bandwidthConfigs = {
        {200, "AIoT_TxSignal_iq_200kHz.bin", {{300, 180, 40.8}, {500, 180, 45.8}}}
    };

    QVERIFY(controller.start(config));
    QTRY_COMPARE(finishCount, 1);
    QCOMPARE(completion.reason, CompletionReason::Completed);
    QCOMPARE(eventOrder, QStringList({"cleanup", "finished"}));
    QCOMPARE(states,
             QList<TestState>({TestState::Validating,
                               TestState::Preparing,
                               TestState::Running,
                               TestState::CleaningUp,
                               TestState::Completed}));
    QCOMPARE(rowCount, 2);
    QCOMPARE(result.rows.size(), 2);
    QVERIFY(result.overallPass);
}

void CoreUtilsTest::case85AsyncCancellationDuringScreenshotDownload()
{
    auto analyzerAudit = std::make_shared<QStringList>();
    auto generatorAudit = std::make_shared<QStringList>();
    Case85RunController controller;
    controller.setTransportFactories(
        [analyzerAudit]() {
            QHash<QString, QList<QByteArray>> responses = case85AnalyzerResponses();
            return new DelayedBinaryBlockTransport(responses, analyzerAudit, 300);
        },
        [generatorAudit]() {
            auto *transport = new AuditedFakeScpiTransport({}, generatorAudit);
            transport->enqueueResponse(":SOURce1:RADio:ARB:SCLock:RATE?", "1920000\n");
            transport->enqueueResponse(":SYSTem:ERRor?", "+0,\"No error\"\n");
            return transport;
        });

    QStringList eventOrder;
    int finishCount = 0;
    TestCompletion completion;
    connect(&controller, &Case85RunController::cleanupCompleted,
            this, [&eventOrder](bool) { eventOrder.append("cleanup"); });
    connect(&controller, &Case85RunController::finished,
            this, [&eventOrder, &finishCount, &completion](const TestCompletion &value) {
        eventOrder.append("finished");
        ++finishCount;
        completion = value;
    });

    Case85RunConfig config;
    config.realInstrumentMode = true;
    config.analyzerHost = "analyzer";
    config.generatorHost = "generator";
    config.txPowerDbm = 0.0;
    config.screenshotRunDir = QDir::temp().filePath("case85-test-screenshots");
    config.frequencyTextByBandwidth.insert(200, "925");
    config.bandwidthConfigs = {
        {200, "AIoT_TxSignal_iq_200kHz.bin", {{300, 180, 40.8}}}
    };

    QVERIFY(controller.start(config));
    QTRY_COMPARE(controller.state(), TestState::Running);
    QTest::qWait(80);
    controller.requestStop();
    QCOMPARE(controller.state(), TestState::Stopping);
    QTRY_COMPARE(finishCount, 1);
    QCOMPARE(completion.reason, CompletionReason::Cancelled);
    QCOMPARE(eventOrder, QStringList({"cleanup", "finished"}));
    QCOMPARE(controller.state(), TestState::Cancelled);
    QVERIFY(analyzerAudit->contains(":ABORt"));
    QVERIFY(generatorAudit->contains(":OUTPut:ALL OFF"));
}

void CoreUtilsTest::case85AsyncScreenshotLocalWriteFailure()
{
    auto analyzerAudit = std::make_shared<QStringList>();
    auto generatorAudit = std::make_shared<QStringList>();
    Case85RunController controller;
    controller.setTransportFactories(
        [analyzerAudit]() {
            QHash<QString, QList<QByteArray>> responses = case85AnalyzerResponses();
            return new DelayedBinaryBlockTransport(responses, analyzerAudit, 0);
        },
        [generatorAudit]() {
            auto *transport = new AuditedFakeScpiTransport({}, generatorAudit);
            transport->enqueueResponse(":SOURce1:RADio:ARB:SCLock:RATE?", "1920000\n");
            transport->enqueueResponse(":SYSTem:ERRor?", "+0,\"No error\"\n");
            return transport;
        });

    QStringList logs;
    int finishCount = 0;
    Case85Result result;
    TestCompletion completion;
    connect(&controller, &Case85RunController::logReady,
            this, [&logs](const QString &level, const QString &source, const QString &message) {
        logs.append(level + "|" + source + "|" + message);
    });
    connect(&controller, &Case85RunController::resultReady,
            this, [&result](const Case85Result &value) { result = value; });
    connect(&controller, &Case85RunController::finished,
            this, [&finishCount, &completion](const TestCompletion &value) {
        ++finishCount;
        completion = value;
    });

    const QString screenshotRoot = QDir::temp().filePath("case85-write-failure-marker");
    QFile markerFile(screenshotRoot);
    QVERIFY(markerFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    markerFile.write("marker");
    markerFile.close();

    Case85RunConfig config;
    config.realInstrumentMode = true;
    config.analyzerHost = "analyzer";
    config.generatorHost = "generator";
    config.txPowerDbm = 0.0;
    config.screenshotRunDir = screenshotRoot;
    config.frequencyTextByBandwidth.insert(200, "925");
    config.bandwidthConfigs = {
        {200, "AIoT_TxSignal_iq_200kHz.bin", {{300, 180, 40.8}}}
    };

    QVERIFY(controller.start(config));
    QTRY_COMPARE(finishCount, 1);
    QCOMPARE(completion.reason, CompletionReason::Completed);
    QCOMPARE(completion.verdict, TestVerdict::Pass);
    QCOMPARE(result.rows.size(), 1);
    QVERIFY(result.rows.first().detail.screenshotPath.isEmpty());
    QVERIFY(std::none_of(analyzerAudit->cbegin(), analyzerAudit->cend(), [](const QString &entry) {
        return entry.startsWith(QStringLiteral(":MMEMory:DELete "));
    }));
    QVERIFY(std::any_of(logs.cbegin(), logs.cend(), [](const QString &entry) {
        return entry.contains(QStringLiteral("ERROR|4071|"))
                && entry.contains(QStringLiteral("保留仪表端文件待人工检查"));
    }));

    QFile::remove(screenshotRoot);
}

void CoreUtilsTest::case82AsyncSpectrumErrorQueueFailure()
{
    auto analyzerAudit = std::make_shared<QStringList>();
    Case82RunController controller;
    controller.setTransportFactories(
        [analyzerAudit]() {
            return new AuditedFakeScpiTransport(
                {{"*IDN?", "Ceyear,4071E,SN,1.0\n"},
                 {"*OPC?", "1\n"},
                 {":SYSTem:ERRor?", "-200,\"Execution error\"\n"}},
                analyzerAudit);
        },
        []() {
            return new AuditedFakeScpiTransport(
                case82GeneratorResponses(),
                std::make_shared<QStringList>());
        });

    QStringList eventOrder;
    int finishCount = 0;
    Case82Result result;
    TestCompletion completion;
    connect(&controller, &Case82RunController::resultReady,
            this, [&result](const Case82Result &value) { result = value; });
    connect(&controller, &Case82RunController::cleanupCompleted,
            this, [&eventOrder](bool) { eventOrder.append("cleanup"); });
    connect(&controller, &Case82RunController::finished,
            this, [&eventOrder, &finishCount, &completion](const TestCompletion &value) {
        eventOrder.append("finished");
        ++finishCount;
        completion = value;
    });

    Case82RunConfig config;
    config.analyzerHost = "analyzer";
    config.generatorHost = "generator";
    config.powerPoints = {0.0};
    config.frequencyMHz = 925.0;
    config.bandwidth = "200";
    config.spanMHz = 1.0;
    QVERIFY(controller.start(config));
    QTRY_COMPARE(finishCount, 1);

    QCOMPARE(eventOrder, QStringList({"cleanup", "finished"}));
    QCOMPARE(completion.reason, CompletionReason::Completed);
    QCOMPARE(completion.verdict, TestVerdict::Fail);
    QCOMPARE(result.rows.size(), 1);
    QVERIFY(!result.rows.first().pass);
    QCOMPARE(result.rows.first().failureReason, QString("4071 配置失败"));
    QCOMPARE(result.rows.first().sampleCount, 0);
    QCOMPARE(analyzerAudit->count(":CALCulate:MARKer1:MAXimum"), 0);
    QCOMPARE(controller.state(), TestState::Completed);
}

QTEST_GUILESS_MAIN(CoreUtilsTest)

#include "tst_coreutils.moc"
