#ifndef CASE82RUNCONTROLLER_H
#define CASE82RUNCONTROLLER_H

#include "case82worker.h"
#include "testtypes.h"

#include <QObject>
#include <memory>

class Case82CancellationToken;
class QThread;

class Case82RunController : public QObject
{
    Q_OBJECT

public:
    explicit Case82RunController(QObject *parent = nullptr);
    ~Case82RunController() override;

    TestState state() const;
    bool isActive() const;
    bool start(const Case82RunConfig &config);
    void requestStop();
    bool waitForFinished(int timeoutMs);

    void setTransportFactories(Case82Worker::TransportFactory analyzerFactory,
                               Case82Worker::TransportFactory generatorFactory);

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
    void initialReady(const Case82RunConfig &config);
    void sampleReady(const Case82Sample &sample);
    void rowReady(int row, const Case82RowResult &result);
    void resultReady(const Case82Result &result);
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
    Case82Worker *m_worker;
    std::unique_ptr<Case82CancellationToken> m_cancellationToken;
    Case82Worker::TransportFactory m_analyzerTransportFactory;
    Case82Worker::TransportFactory m_generatorTransportFactory;
    bool m_finishedEmitted;
    bool m_cleanupCompleted;
};

#endif
