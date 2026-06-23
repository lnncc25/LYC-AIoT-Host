#ifndef CASE85WORKER_H
#define CASE85WORKER_H

#include "case85model.h"
#include "icase85resultpresenter.h"
#include "itesteventsink.h"
#include "testtypes.h"

#include <QObject>
#include <functional>

class Case85CancellationToken;
class IScpiTransport;

class Case85Worker : public QObject,
                     public ITestEventSink,
                     public ICase85ResultPresenter
{
    Q_OBJECT

public:
    using TransportFactory = std::function<IScpiTransport *()>;

    Case85Worker(const Case85RunConfig &config,
                 Case85CancellationToken *cancellationToken,
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
    void initialReady(int totalRows);
    void rowReady(int row, const Case85RowResult &result);
    void resultReady(const Case85Result &result);
    void cleanupStarted();
    void cleanupFinished(bool safe);
    void executionFinished(const TestCompletion &completion);

private:
    void addTestLog(const QString &level,
                    const QString &source,
                    const QString &message) override;
    void presentCase85Initial(int totalRows) override;
    void presentCase85Row(int row, const Case85RowResult &result) override;
    void presentCase85Result(const Case85Result &result) override;

    Case85RunConfig m_config;
    Case85CancellationToken *m_cancellationToken;
    TransportFactory m_analyzerTransportFactory;
    TransportFactory m_generatorTransportFactory;
};

#endif
