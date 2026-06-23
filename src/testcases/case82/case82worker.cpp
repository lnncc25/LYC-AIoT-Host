#include "case82worker.h"

#include "analyzer4071.h"
#include "case82cancellationtoken.h"
#include "generator1466.h"
#include "instrumentsession.h"
#include "iscpitransport.h"
#include "testcase82.h"

#include <memory>

namespace {

bool connectSession(InstrumentSession &session,
                    const QString &host,
                    quint16 port,
                    Case82CancellationToken *token)
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

Case82Worker::Case82Worker(const Case82RunConfig &config,
                           Case82CancellationToken *cancellationToken,
                           TransportFactory analyzerTransportFactory,
                           TransportFactory generatorTransportFactory)
    : m_config(config)
    , m_cancellationToken(cancellationToken)
    , m_analyzerTransportFactory(std::move(analyzerTransportFactory))
    , m_generatorTransportFactory(std::move(generatorTransportFactory))
{
}

void Case82Worker::execute()
{
    emit executionStarted();

    std::unique_ptr<InstrumentSession> analyzerSession;
    std::unique_ptr<InstrumentSession> generatorSession;
    std::unique_ptr<Analyzer4071> analyzer;
    std::unique_ptr<Generator1466> generator;
    TestCompletion completion{CompletionReason::ExecutionFailed,
                              TestVerdict::NotRun,
                              QStringLiteral("instrument connection failed")};

    if (!m_cancellationToken->isCancellationRequested()) {
        analyzerSession.reset(new InstrumentSession(m_analyzerTransportFactory()));
        connect(analyzerSession.get(), &InstrumentSession::commandSent,
                this, [this](const QString &command) {
            emit logReady(QStringLiteral("INFO"), QStringLiteral("4071"),
                          QStringLiteral("发送: ") + command);
        });
        connect(analyzerSession.get(), &InstrumentSession::commandSendFailed,
                this, [this](const QString &error) {
            emit logReady(QStringLiteral("ERROR"), QStringLiteral("4071"),
                          QStringLiteral("发送失败: ") + error);
        });
        if (connectSession(*analyzerSession,
                           m_config.analyzerHost,
                           m_config.analyzerPort,
                           m_cancellationToken)) {
            analyzer.reset(new Analyzer4071(analyzerSession.get()));
        }
    }

    if (!m_cancellationToken->isCancellationRequested()) {
        generatorSession.reset(new InstrumentSession(m_generatorTransportFactory()));
        connect(generatorSession.get(), &InstrumentSession::commandSent,
                this, [this](const QString &command) {
            emit logReady(QStringLiteral("INFO"), QStringLiteral("1466"),
                          QStringLiteral("发送: ") + command);
        });
        connect(generatorSession.get(), &InstrumentSession::commandSendFailed,
                this, [this](const QString &error) {
            emit logReady(QStringLiteral("ERROR"), QStringLiteral("1466"),
                          QStringLiteral("发送失败: ") + error);
        });
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
    } else if (analyzer && generator) {
        TestCase82 testCase(analyzer.get(),
                            generator.get(),
                            m_config,
                            this,
                            this,
                            m_cancellationToken);
        completion = testCase.start();
    }

    emit cleanupStarted();
    bool cleanupSafe = true;
    if (analyzer) {
        emit logReady(QStringLiteral("INFO"), QStringLiteral("4071"),
                      QStringLiteral("8.2 测试结束停止测量，停止测量/扫描"));
        cleanupSafe = analyzer->stopMeasurement() && cleanupSafe;
    }
    if (generator) {
        emit logReady(QStringLiteral("INFO"), QStringLiteral("1466"),
                      QStringLiteral("8.2 测试结束安全关断，关闭 RF 输出"));
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

void Case82Worker::addTestLog(const QString &level,
                              const QString &source,
                              const QString &message)
{
    emit logReady(level, source, message);
}

void Case82Worker::presentCase82Initial(const Case82RunConfig &config)
{
    emit summaryReady(QStringLiteral("8.2 输出功率 - 真实仪表联调"),
                      QString::number(config.frequencyMHz, 'f', 3) + QStringLiteral(" MHz"),
                      config.bandwidth + QStringLiteral(" kHz"),
                      0,
                      QStringLiteral("8.2 输出功率 %p%"));
    emit initialReady(config);
}

void Case82Worker::presentCase82Sample(const Case82Sample &sample)
{
    emit sampleReady(sample);
}

void Case82Worker::presentCase82Row(int row, const Case82RowResult &result)
{
    emit rowReady(row, result);
}

void Case82Worker::presentCase82Result(const Case82Result &result)
{
    emit resultReady(result);
}
