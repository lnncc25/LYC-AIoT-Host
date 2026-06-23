#include "csvutils.h"
#include "logentry.h"
#include "outputpaths.h"
#include "testtypes.h"

#include <QDir>
#include <QtTest>

class CoreUtilsTest : public QObject
{
    Q_OBJECT

private slots:
    void escapeCsvCell_data();
    void escapeCsvCell();
    void outputPaths();
    void testTypeNames();
    void valueEquality();
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

QTEST_APPLESS_MAIN(CoreUtilsTest)

#include "tst_coreutils.moc"
