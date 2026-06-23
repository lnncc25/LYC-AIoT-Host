#include "analyzer4071.h"
#include "case81runconfig.h"
#include "case81runcontroller.h"
#include "case81model.h"
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
#include <QtTest>
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
    void case81DualInstrument();
    void case81LegacyInstrument();
    void case81RegistryAndRepeat();
    void case81AsyncLifecycle();
    void case81AsyncCancellation();
    void case81AsyncCleanupFailure();
    void case81AsyncRepeat();
};

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

QTEST_GUILESS_MAIN(CoreUtilsTest)

#include "tst_coreutils.moc"
