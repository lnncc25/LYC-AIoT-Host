#include "analyzer4071.h"
#include "case81model.h"
#include "generator1466.h"
#include "icase81resultpresenter.h"
#include "instrumentsession.h"
#include "itestcaseview.h"
#include "itesteventsink.h"
#include "tcpscpitransport.h"
#include "testcase81.h"
#include "testtypes.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTextStream>
#include <QTimer>

namespace {

class ProbeCapture : public ITestCaseView,
                     public ITestEventSink,
                     public ICase81ResultPresenter
{
public:
    void showCaseSummary(const QString &title,
                         const QString &,
                         const QString &,
                         int,
                         const QString &) override
    {
        summaryTitle = title;
    }

    void addTestLog(const QString &level,
                    const QString &source,
                    const QString &message) override
    {
        logs.append(level + "|" + source + "|" + message);
    }

    void presentCase81(const Case81Result &value) override
    {
        result = value;
        presented = true;
    }

    QString summaryTitle;
    QStringList logs;
    Case81Result result;
    bool presented = false;
};

bool connectSession(InstrumentSession &session,
                    const QString &host,
                    quint16 port,
                    QString *error)
{
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&session, &InstrumentSession::connected,
                     &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout,
                     &loop, &QEventLoop::quit);

    session.connectToHost(host, port);
    timeout.start(3000);
    loop.exec();
    if (session.isConnected()) {
        return true;
    }

    *error = session.errorString();
    return false;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList arguments = app.arguments();
    if (arguments.size() != 5) {
        QTextStream(stderr)
            << "Usage: case81_probe <analyzer-host> <analyzer-port> "
               "<generator-host> <generator-port>\n";
        return 2;
    }

    bool analyzerPortOk = false;
    bool generatorPortOk = false;
    const uint analyzerPort = arguments.at(2).toUInt(&analyzerPortOk);
    const uint generatorPort = arguments.at(4).toUInt(&generatorPortOk);
    if (!analyzerPortOk || analyzerPort > 65535
        || !generatorPortOk || generatorPort > 65535) {
        QTextStream(stderr) << "Invalid port\n";
        return 2;
    }

    InstrumentSession analyzerSession(new TcpScpiTransport);
    InstrumentSession generatorSession(new TcpScpiTransport);
    QString connectError;
    if (!connectSession(analyzerSession,
                        arguments.at(1),
                        static_cast<quint16>(analyzerPort),
                        &connectError)) {
        QTextStream(stderr) << "Analyzer connection failed: " << connectError << '\n';
        return 3;
    }
    if (!connectSession(generatorSession,
                        arguments.at(3),
                        static_cast<quint16>(generatorPort),
                        &connectError)) {
        analyzerSession.disconnectFromHost();
        QTextStream(stderr) << "Generator connection failed: " << connectError << '\n';
        return 3;
    }

    Analyzer4071 analyzer(&analyzerSession);
    Generator1466 generator(&generatorSession);
    ProbeCapture capture;
    TestCase81 testCase(&analyzer, &generator, &capture, &capture, &capture);
    const TestCompletion completion = testCase.start();

    analyzerSession.disconnectFromHost();
    generatorSession.disconnectFromHost();

    QTextStream output(stdout);
    output << "SUMMARY=" << capture.summaryTitle << '\n'
           << "COMPLETION=" << completionReasonName(completion.reason) << '\n'
           << "VERDICT=" << testVerdictName(completion.verdict) << '\n'
           << "ANALYZER_IDN=" << capture.result.analyzerIdn << '\n'
           << "GENERATOR_IDN=" << capture.result.generatorIdn << '\n'
           << "ANALYZER_ERROR=" << capture.result.analyzerError << '\n'
           << "GENERATOR_ERROR=" << capture.result.generatorError << '\n';

    if (!capture.presented
        || !capture.result.dualInstrumentMode
        || completion.reason != CompletionReason::Completed
        || completion.verdict != TestVerdict::Pass) {
        return 4;
    }
    return 0;
}
