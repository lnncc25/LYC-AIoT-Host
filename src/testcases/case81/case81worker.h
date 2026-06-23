#ifndef CASE81WORKER_H
#define CASE81WORKER_H

#include "case81runconfig.h"
#include "icase81resultpresenter.h"
#include "itestcaseview.h"
#include "itesteventsink.h"
#include "testtypes.h"

#include <QObject>
#include <functional>

class Case81CancellationToken;
class IScpiTransport;

class Case81Worker : public QObject,
                     public ITestCaseView,
                     public ITestEventSink,
                     public ICase81ResultPresenter
{
    Q_OBJECT

public:
    using TransportFactory = std::function<IScpiTransport *()>;

    Case81Worker(const Case81RunConfig &config,
                 Case81CancellationToken *cancellationToken,
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
    void resultReady(const Case81Result &result);
    void cleanupStarted();
    void cleanupFinished(bool safe);
    void executionFinished(const TestCompletion &completion);

private:
    void showCaseSummary(const QString &title,
                         const QString &frequency,
                         const QString &bandwidth,
                         int progress,
                         const QString &progressFormat) override;
    void addTestLog(const QString &level,
                    const QString &source,
                    const QString &message) override;
    void presentCase81(const Case81Result &result) override;

    Case81RunConfig m_config;
    Case81CancellationToken *m_cancellationToken;
    TransportFactory m_analyzerTransportFactory;
    TransportFactory m_generatorTransportFactory;
};

#endif
