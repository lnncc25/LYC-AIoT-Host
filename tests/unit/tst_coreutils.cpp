#include "analyzer4071.h"
#include "csvutils.h"
#include "fakescpitransport.h"
#include "generator1466.h"
#include "instrumentsession.h"
#include "logentry.h"
#include "outputpaths.h"
#include "testtypes.h"

#include <QDir>
#include <QtTest>
#include <type_traits>

static_assert(std::has_virtual_destructor<IScpiTransport>::value,
              "IScpiTransport must be safely replaceable");

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

QTEST_APPLESS_MAIN(CoreUtilsTest)

#include "tst_coreutils.moc"
