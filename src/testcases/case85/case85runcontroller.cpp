#include "case85runcontroller.h"

#include "case85cancellationtoken.h"
#include "tcpscpitransport.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QThread>

Case85RunController::Case85RunController(QObject *parent)
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
    qRegisterMetaType<Case85RunConfig>("Case85RunConfig");
    qRegisterMetaType<Case85RowResult>("Case85RowResult");
    qRegisterMetaType<Case85Result>("Case85Result");
}

Case85RunController::~Case85RunController()
{
    requestStop();
    waitForFinished(8000);
    destroyRunObjects();
}

TestState Case85RunController::state() const
{
    return m_state;
}

bool Case85RunController::isActive() const
{
    return m_state == TestState::Validating
            || m_state == TestState::Preparing
            || m_state == TestState::Running
            || m_state == TestState::Stopping
            || m_state == TestState::CleaningUp;
}

bool Case85RunController::start(const Case85RunConfig &config)
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
    m_cancellationToken.reset(new Case85CancellationToken);

    setState(TestState::Validating);
    setState(TestState::Preparing);

    m_thread = new QThread;
    m_worker = new Case85Worker(config,
                                m_cancellationToken.get(),
                                m_analyzerTransportFactory,
                                m_generatorTransportFactory);
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_worker, &Case85Worker::execute);
    connect(m_worker, &Case85Worker::executionStarted,
            this, &Case85RunController::onExecutionStarted);
    connect(m_worker, &Case85Worker::summaryReady,
            this, &Case85RunController::summaryReady);
    connect(m_worker, &Case85Worker::logReady,
            this, &Case85RunController::logReady);
    connect(m_worker, &Case85Worker::initialReady,
            this, &Case85RunController::initialReady);
    connect(m_worker, &Case85Worker::rowReady,
            this, &Case85RunController::rowReady);
    connect(m_worker, &Case85Worker::resultReady,
            this, &Case85RunController::resultReady);
    connect(m_worker, &Case85Worker::cleanupStarted,
            this, &Case85RunController::onCleanupStarted);
    connect(m_worker, &Case85Worker::cleanupFinished,
            this, &Case85RunController::onCleanupFinished);
    connect(m_worker, &Case85Worker::executionFinished,
            this, &Case85RunController::onExecutionFinished);
    connect(m_worker, &Case85Worker::executionFinished,
            m_thread, &QThread::quit);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);
    m_thread->start();
    return true;
}

void Case85RunController::requestStop()
{
    if (!isActive() || !m_cancellationToken) {
        return;
    }
    m_cancellationToken->requestCancellation();
    if (m_state != TestState::Stopping && m_state != TestState::CleaningUp) {
        setState(TestState::Stopping);
    }
}

bool Case85RunController::waitForFinished(int timeoutMs)
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

void Case85RunController::setTransportFactories(
    Case85Worker::TransportFactory analyzerFactory,
    Case85Worker::TransportFactory generatorFactory)
{
    if (isActive()) {
        return;
    }
    m_analyzerTransportFactory = std::move(analyzerFactory);
    m_generatorTransportFactory = std::move(generatorFactory);
}

void Case85RunController::onExecutionStarted()
{
    if (m_state == TestState::Preparing) {
        setState(TestState::Running);
    }
}

void Case85RunController::onCleanupStarted()
{
    setState(TestState::CleaningUp);
}

void Case85RunController::onCleanupFinished(bool safe)
{
    m_cleanupCompleted = true;
    emit cleanupCompleted(safe);
}

void Case85RunController::onExecutionFinished(const TestCompletion &completion)
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

void Case85RunController::setState(TestState state)
{
    if (m_state == state) {
        return;
    }
    m_state = state;
    emit stateChanged(state);
}

void Case85RunController::destroyRunObjects()
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
