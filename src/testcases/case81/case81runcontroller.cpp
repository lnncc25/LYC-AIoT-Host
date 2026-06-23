#include "case81runcontroller.h"

#include "case81cancellationtoken.h"
#include "case81model.h"
#include "tcpscpitransport.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

Case81RunController::Case81RunController(QObject *parent)
    : QObject(parent)
    , m_state(TestState::Idle)
    , m_thread(nullptr)
    , m_worker(nullptr)
    , m_analyzerTransportFactory([]() { return new TcpScpiTransport; })
    , m_generatorTransportFactory([]() { return new TcpScpiTransport; })
    , m_finishedEmitted(false)
    , m_cleanupCompleted(false)
{
    qRegisterMetaType<TestState>("TestState");
    qRegisterMetaType<TestCompletion>("TestCompletion");
    qRegisterMetaType<Case81Result>("Case81Result");
}

Case81RunController::~Case81RunController()
{
    requestStop();
    waitForFinished(8000);
    destroyRunObjects();
}

TestState Case81RunController::state() const
{
    return m_state;
}

bool Case81RunController::isActive() const
{
    return m_state == TestState::Validating
            || m_state == TestState::Preparing
            || m_state == TestState::Running
            || m_state == TestState::Stopping
            || m_state == TestState::CleaningUp;
}

bool Case81RunController::start(const Case81RunConfig &config)
{
    if (isActive() || !config.hasAnalyzer()) {
        return false;
    }
    if (m_state != TestState::Idle) {
        setState(TestState::Idle);
    }

    destroyRunObjects();
    m_finishedEmitted = false;
    m_cleanupCompleted = false;
    m_cancellationToken.reset(new Case81CancellationToken);

    setState(TestState::Validating);
    setState(TestState::Preparing);

    m_thread = new QThread;
    m_worker = new Case81Worker(config,
                                m_cancellationToken.get(),
                                m_analyzerTransportFactory,
                                m_generatorTransportFactory);
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_worker, &Case81Worker::execute);
    connect(m_worker, &Case81Worker::executionStarted,
            this, &Case81RunController::onExecutionStarted);
    connect(m_worker, &Case81Worker::summaryReady,
            this, &Case81RunController::summaryReady);
    connect(m_worker, &Case81Worker::logReady,
            this, &Case81RunController::logReady);
    connect(m_worker, &Case81Worker::resultReady,
            this, &Case81RunController::resultReady);
    connect(m_worker, &Case81Worker::cleanupStarted,
            this, &Case81RunController::onCleanupStarted);
    connect(m_worker, &Case81Worker::cleanupFinished,
            this, &Case81RunController::onCleanupFinished);
    connect(m_worker, &Case81Worker::executionFinished,
            this, &Case81RunController::onExecutionFinished);
    connect(m_worker, &Case81Worker::executionFinished,
            m_thread, &QThread::quit);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread->start();
    return true;
}

void Case81RunController::requestStop()
{
    if (!isActive() || !m_cancellationToken) {
        return;
    }
    m_cancellationToken->requestCancellation();
    if (m_state != TestState::Stopping && m_state != TestState::CleaningUp) {
        setState(TestState::Stopping);
    }
}

bool Case81RunController::waitForFinished(int timeoutMs)
{
    QElapsedTimer timer;
    timer.start();
    while (isActive() && timer.elapsed() < timeoutMs) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        QThread::msleep(5);
    }
    if (m_thread && m_thread->isRunning()) {
        m_thread->quit();
        return m_thread->wait(qMax(0, timeoutMs - int(timer.elapsed())));
    }
    return !isActive();
}

void Case81RunController::setTransportFactories(
    Case81Worker::TransportFactory analyzerFactory,
    Case81Worker::TransportFactory generatorFactory)
{
    if (isActive()) {
        return;
    }
    m_analyzerTransportFactory = std::move(analyzerFactory);
    m_generatorTransportFactory = std::move(generatorFactory);
}

void Case81RunController::onExecutionStarted()
{
    if (m_state == TestState::Preparing) {
        setState(TestState::Running);
    }
}

void Case81RunController::onCleanupStarted()
{
    setState(TestState::CleaningUp);
}

void Case81RunController::onCleanupFinished(bool safe)
{
    m_cleanupCompleted = true;
    emit cleanupCompleted(safe);
}

void Case81RunController::onExecutionFinished(const TestCompletion &completion)
{
    if (m_finishedEmitted || !m_cleanupCompleted) {
        return;
    }
    m_finishedEmitted = true;
    switch (completion.reason) {
    case CompletionReason::Completed:
        setState(TestState::Completed);
        break;
    case CompletionReason::ExecutionFailed:
        setState(TestState::ExecutionFailed);
        break;
    case CompletionReason::Cancelled:
        setState(TestState::Cancelled);
        break;
    }
    emit finished(completion);
}

void Case81RunController::setState(TestState state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit stateChanged(state);
}

void Case81RunController::destroyRunObjects()
{
    if (!m_thread) {
        return;
    }
    if (m_thread->isRunning()) {
        m_thread->quit();
        m_thread->wait();
    }
    delete m_thread;
    m_thread = nullptr;
    m_worker = nullptr;
    m_cancellationToken.reset();
}
