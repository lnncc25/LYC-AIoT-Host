#ifndef CASE81RUNCONTROLLER_H
#define CASE81RUNCONTROLLER_H

#include "case81runconfig.h"
#include "case81worker.h"
#include "testtypes.h"

#include <QObject>
#include <memory>

class Case81CancellationToken;
class QThread;

class Case81RunController : public QObject
{
    Q_OBJECT

public:
    explicit Case81RunController(QObject *parent = nullptr);
    ~Case81RunController() override;

    TestState state() const;
    bool isActive() const;
    bool start(const Case81RunConfig &config);
    void requestStop();
    bool waitForFinished(int timeoutMs);

    void setTransportFactories(Case81Worker::TransportFactory analyzerFactory,
                               Case81Worker::TransportFactory generatorFactory);

signals:
    void stateChanged(TestState state);
    void summaryReady(const QString &title,
                      const QString &frequency,
                      const QString &bandwidth,
                      int progress,
                      const QString &progressFormat);
    void logReady(const QString &level,
                  const QString &source,
                  const QString &message);
    void resultReady(const Case81Result &result);
    void cleanupCompleted(bool safe);
    void finished(const TestCompletion &completion);

private slots:
    void onExecutionStarted();
    void onCleanupStarted();
    void onCleanupFinished(bool safe);
    void onExecutionFinished(const TestCompletion &completion);

private:
    void setState(TestState state);
    void destroyRunObjects();

    TestState m_state;
    QThread *m_thread;
    Case81Worker *m_worker;
    std::unique_ptr<Case81CancellationToken> m_cancellationToken;
    Case81Worker::TransportFactory m_analyzerTransportFactory;
    Case81Worker::TransportFactory m_generatorTransportFactory;
    bool m_finishedEmitted;
    bool m_cleanupCompleted;
};

#endif
