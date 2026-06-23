#include "case81worker.h"

#include "analyzer4071.h"
#include "case81cancellationtoken.h"
#include "generator1466.h"
#include "instrumentsession.h"
#include "iscpitransport.h"
#include "testcase81.h"

#include <memory>

namespace {

bool connectSession(InstrumentSession &session,
                    const QString &host,
                    quint16 port,
                    Case81CancellationToken *token)
{
    session.connectToHost(host, port);
    for (int elapsed = 0; elapsed < 3000; elapsed += 100) {
        if (session.isConnected()) {
            return true;
        }
        if (token->isCancellationRequested()) {
            return false;
        }
        session.waitForConnected(100);
    }
    return session.isConnected();
}

}

Case81Worker::Case81Worker(const Case81RunConfig &config,
                           Case81CancellationToken *cancellationToken,
                           TransportFactory analyzerTransportFactory,
                           TransportFactory generatorTransportFactory)
    : m_config(config)
    , m_cancellationToken(cancellationToken)
    , m_analyzerTransportFactory(std::move(analyzerTransportFactory))
    , m_generatorTransportFactory(std::move(generatorTransportFactory))
{
}

void Case81Worker::execute()
{
    emit executionStarted();

    std::unique_ptr<InstrumentSession> analyzerSession;
    std::unique_ptr<InstrumentSession> generatorSession;
    std::unique_ptr<Analyzer4071> analyzer;
    std::unique_ptr<Generator1466> generator;
    TestCompletion completion{CompletionReason::ExecutionFailed,
                              TestVerdict::NotRun,
                              QStringLiteral("4071 analyzer connection failed")};

    if (!m_cancellationToken->isCancellationRequested()) {
        analyzerSession.reset(new InstrumentSession(m_analyzerTransportFactory()));
        if (connectSession(*analyzerSession,
                           m_config.analyzerHost,
                           m_config.analyzerPort,
                           m_cancellationToken)) {
            analyzer.reset(new Analyzer4071(analyzerSession.get()));
        }
    }

    if (analyzer && m_config.hasGenerator()
        && !m_cancellationToken->isCancellationRequested()) {
        generatorSession.reset(new InstrumentSession(m_generatorTransportFactory()));
        if (connectSession(*generatorSession,
                           m_config.generatorHost,
                           m_config.generatorPort,
                           m_cancellationToken)) {
            generator.reset(new Generator1466(generatorSession.get()));
        }
    }

    if (m_cancellationToken->isCancellationRequested()) {
        completion = {CompletionReason::Cancelled,
                      TestVerdict::NotRun,
                      QStringLiteral("stop requested")};
    } else if (analyzer) {
        TestCase81 testCase(analyzer.get(),
                            generator.get(),
                            this,
                            this,
                            this,
                            m_cancellationToken);
        completion = testCase.start();
    }

    emit cleanupStarted();
    bool cleanupSafe = true;
    if (analyzer) {
        cleanupSafe = analyzer->stopMeasurement() && cleanupSafe;
    }
    if (generator) {
        cleanupSafe = generator->shutdownOutput() && cleanupSafe;
    }
    if (analyzerSession) {
        analyzerSession->disconnectFromHost();
    }
    if (generatorSession) {
        generatorSession->disconnectFromHost();
    }
    emit cleanupFinished(cleanupSafe);

    if (!cleanupSafe) {
        completion.reason = CompletionReason::ExecutionFailed;
        completion.detail = QStringLiteral("instrument safety cleanup failed");
    } else if (m_cancellationToken->isCancellationRequested()) {
        completion.reason = CompletionReason::Cancelled;
        completion.verdict = TestVerdict::NotRun;
        completion.detail = QStringLiteral("stop requested");
    }
    emit executionFinished(completion);
}

void Case81Worker::showCaseSummary(const QString &title,
                                   const QString &frequency,
                                   const QString &bandwidth,
                                   int progress,
                                   const QString &progressFormat)
{
    emit summaryReady(title, frequency, bandwidth, progress, progressFormat);
}

void Case81Worker::addTestLog(const QString &level,
                              const QString &source,
                              const QString &message)
{
    emit logReady(level, source, message);
}

void Case81Worker::presentCase81(const Case81Result &result)
{
    emit resultReady(result);
}
