#include "case82runcontroller.h"

#include "case82cancellationtoken.h"
#include "tcpscpitransport.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

Case82RunController::Case82RunController(QObject *parent)
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
    qRegisterMetaType<Case82RunConfig>("Case82RunConfig");
    qRegisterMetaType<Case82Sample>("Case82Sample");
    qRegisterMetaType<Case82RowResult>("Case82RowResult");
    qRegisterMetaType<Case82Result>("Case82Result");
}

Case82RunController::~Case82RunController()
{
    requestStop();
    waitForFinished(8000);
    destroyRunObjects();
}

TestState Case82RunController::state() const
{
    return m_state;
}

bool Case82RunController::isActive() const
{
    return m_state == TestState::Validating
            || m_state == TestState::Preparing
            || m_state == TestState::Running
            || m_state == TestState::Stopping
            || m_state == TestState::CleaningUp;
}

bool Case82RunController::start(const Case82RunConfig &config)
{
    if (isActive() || !config.isValid()) {
        return false;
    }
    if (m_state != TestState::Idle) {
        setState(TestState::Idle);
    }

    destroyRunObjects();
    m_finishedEmitted = false;
    m_cleanupCompleted = false;
    m_cancellationToken.reset(new Case82CancellationToken);

    setState(TestState::Validating);
    setState(TestState::Preparing);

    m_thread = new QThread;
    m_worker = new Case82Worker(config,
                                m_cancellationToken.get(),
                                m_analyzerTransportFactory,
                                m_generatorTransportFactory);
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_worker, &Case82Worker::execute);
    connect(m_worker, &Case82Worker::executionStarted,
            this, &Case82RunController::onExecutionStarted);
    connect(m_worker, &Case82Worker::summaryReady,
            this, &Case82RunController::summaryReady);
    connect(m_worker, &Case82Worker::logReady,
            this, &Case82RunController::logReady);
    connect(m_worker, &Case82Worker::initialReady,
            this, &Case82RunController::initialReady);
    connect(m_worker, &Case82Worker::sampleReady,
            this, &Case82RunController::sampleReady);
    connect(m_worker, &Case82Worker::rowReady,
            this, &Case82RunController::rowReady);
    connect(m_worker, &Case82Worker::resultReady,
            this, &Case82RunController::resultReady);
    connect(m_worker, &Case82Worker::cleanupStarted,
            this, &Case82RunController::onCleanupStarted);
    connect(m_worker, &Case82Worker::cleanupFinished,
            this, &Case82RunController::onCleanupFinished);
    connect(m_worker, &Case82Worker::executionFinished,
            this, &Case82RunController::onExecutionFinished);
    connect(m_worker, &Case82Worker::executionFinished,
            m_thread, &QThread::quit);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread->start();
    return true;
}

void Case82RunController::requestStop()
{
    if (!isActive() || !m_cancellationToken) {
        return;
    }
    m_cancellationToken->requestCancellation();
    if (m_state != TestState::Stopping && m_state != TestState::CleaningUp) {
        setState(TestState::Stopping);
    }
}

bool Case82RunController::waitForFinished(int timeoutMs)
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

void Case82RunController::setTransportFactories(
    Case82Worker::TransportFactory analyzerFactory,
    Case82Worker::TransportFactory generatorFactory)
{
    if (isActive()) {
        return;
    }
    m_analyzerTransportFactory = std::move(analyzerFactory);
    m_generatorTransportFactory = std::move(generatorFactory);
}

void Case82RunController::onExecutionStarted()
{
    if (m_state == TestState::Preparing) {
        setState(TestState::Running);
    }
}

void Case82RunController::onCleanupStarted()
{
    setState(TestState::CleaningUp);
}

void Case82RunController::onCleanupFinished(bool safe)
{
    m_cleanupCompleted = true;
    emit cleanupCompleted(safe);
}

void Case82RunController::onExecutionFinished(const TestCompletion &completion)
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

void Case82RunController::setState(TestState state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit stateChanged(state);
}

void Case82RunController::destroyRunObjects()
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
