#ifndef CASE85RUNCONTROLLER_H
#define CASE85RUNCONTROLLER_H

#include "case85worker.h"
#include "testtypes.h"

#include <QObject>
#include <memory>

class Case85CancellationToken;
class QThread;

class Case85RunController : public QObject
{
    Q_OBJECT

public:
    explicit Case85RunController(QObject *parent = nullptr);
    ~Case85RunController() override;

    TestState state() const;
    bool isActive() const;
    bool start(const Case85RunConfig &config);
    void requestStop();
    bool waitForFinished(int timeoutMs);

    void setTransportFactories(Case85Worker::TransportFactory analyzerFactory,
                               Case85Worker::TransportFactory generatorFactory);

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
    void initialReady(int totalRows);
    void rowReady(int row, const Case85RowResult &result);
    void resultReady(const Case85Result &result);
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
    Case85Worker *m_worker;
    std::unique_ptr<Case85CancellationToken> m_cancellationToken;
    Case85Worker::TransportFactory m_analyzerTransportFactory;
    Case85Worker::TransportFactory m_generatorTransportFactory;
    bool m_finishedEmitted;
    bool m_cleanupCompleted;
};

#endif
