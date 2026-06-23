#ifndef CASE82WORKER_H
#define CASE82WORKER_H

#include "case82model.h"
#include "icase82resultpresenter.h"
#include "itesteventsink.h"
#include "testtypes.h"

#include <QObject>
#include <functional>

class Case82CancellationToken;
class IScpiTransport;

class Case82Worker : public QObject,
                     public ITestEventSink,
                     public ICase82ResultPresenter
{
    Q_OBJECT

public:
    using TransportFactory = std::function<IScpiTransport *()>;

    Case82Worker(const Case82RunConfig &config,
                 Case82CancellationToken *cancellationToken,
                 TransportFactory analyzerTransportFactory,
                 TransportFactory generatorTransportFactory);

public slots:
    void execute();

signals:
    void executionStarted();
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
    void cleanupStarted();
    void cleanupFinished(bool safe);
    void executionFinished(const TestCompletion &completion);

private:
    void addTestLog(const QString &level,
                    const QString &source,
                    const QString &message) override;
    void presentCase82Initial(const Case82RunConfig &config) override;
    void presentCase82Sample(const Case82Sample &sample) override;
    void presentCase82Row(int row, const Case82RowResult &result) override;
    void presentCase82Result(const Case82Result &result) override;

    Case82RunConfig m_config;
    Case82CancellationToken *m_cancellationToken;
    TransportFactory m_analyzerTransportFactory;
    TransportFactory m_generatorTransportFactory;
};

#endif
