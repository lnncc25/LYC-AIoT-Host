#include "testcase81.h"

#include "analyzer4071.h"
#include "case81model.h"
#include "generator1466.h"
#include "icase81resultpresenter.h"
#include "itestcaseview.h"
#include "itesteventsink.h"

#include <QThread>

TestCase81::TestCase81(Analyzer4071 *analyzer,
                       Generator1466 *generator,
                       ITestCaseView *view,
                       ITestEventSink *eventSink,
                       ICase81ResultPresenter *resultPresenter)
    : m_analyzer(analyzer)
    , m_generator(generator)
    , m_view(view)
    , m_eventSink(eventSink)
    , m_resultPresenter(resultPresenter)
{
}

TestCaseId TestCase81::id() const
{
    return TestCaseId::Case81;
}

QString TestCase81::displayName() const
{
    return QStringLiteral("8.1 基本功能验证");
}

bool TestCase81::canStart() const
{
    return m_analyzer && m_analyzer->isConnected();
}

TestCompletion TestCase81::start()
{
    if (!canStart()) {
        return {CompletionReason::ExecutionFailed,
                TestVerdict::NotRun,
                QStringLiteral("4071 analyzer is not connected")};
    }

    m_view->showCaseSummary(displayName(),
                            QStringLiteral("---"),
                            QStringLiteral("---"),
                            100,
                            QStringLiteral("8.1 基本功能 %p%"));

    Case81Result result;
    result.dualInstrumentMode = m_generator && m_generator->isConnected();
    if (result.dualInstrumentMode) {
        result.analyzerIdn = responseText(m_analyzer->identify(), "4071", "*IDN?");
        result.generatorIdn = responseText(m_generator->identify(), "1466", "*IDN?");
        result.analyzerError = responseText(m_analyzer->readError(), "4071", ":SYSTem:ERRor?");
        result.generatorError = responseText(m_generator->readError(), "1466", ":SYSTem:ERRor?");
        result.analyzerIdentified = !result.analyzerIdn.isEmpty();
        result.generatorIdentified = !result.generatorIdn.isEmpty();
        result.overallPass = result.analyzerIdentified && result.generatorIdentified;
        m_resultPresenter->presentCase81(result);
        m_eventSink->addTestLog(result.overallPass ? "PASS" : "FAIL",
                                "测试",
                                "8.1 双仪表基本通信验证完成");
    } else {
        result.analyzerIdn = responseText(m_analyzer->identify(), "4071", "*IDN?", true);
        QThread::msleep(200);
        const ScpiReply voltageReply = m_analyzer->measureBasicVoltage();
        const QString voltageText =
            responseText(voltageReply, "4071", "MEAS:VOLT:DC?", true);
        result.analyzerIdentified = !result.analyzerIdn.isEmpty();
        result.voltage = voltageText.toDouble(&result.voltageAvailable);
        result.overallPass = result.voltageAvailable && result.analyzerIdentified;
        m_resultPresenter->presentCase81(result);
        m_eventSink->addTestLog(result.overallPass ? "PASS" : "FAIL",
                                "测试",
                                "8.1 基本功能验证完成");
    }

    return {CompletionReason::Completed,
            result.overallPass ? TestVerdict::Pass : TestVerdict::Fail,
            QString()};
}

void TestCase81::requestStop()
{
    // Stage 4 remains synchronous; repeated stop requests intentionally have no effect.
}

QString TestCase81::responseText(const ScpiReply &reply,
                                 const QString &source,
                                 const QString &command,
                                 bool receivedStyle) const
{
    if (!reply.isSuccess()) {
        m_eventSink->addTestLog("WARN", source, "查询超时: " + command);
        return QString();
    }

    m_eventSink->addTestLog("INFO",
                            source,
                            (receivedStyle ? "收到: " : "查询返回: ") + reply.text);
    return reply.text;
}
