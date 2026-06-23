#ifndef TESTCASE85_H
#define TESTCASE85_H

#include "case85model.h"
#include "itestcase.h"

class Analyzer4071;
class Case85CancellationToken;
class Generator1466;
class ICase85ResultPresenter;
class ITestEventSink;

class TestCase85 : public ITestCase
{
public:
    TestCase85(Analyzer4071 *analyzer,
               Generator1466 *generator,
               const Case85RunConfig &config,
               ITestEventSink *eventSink,
               ICase85ResultPresenter *presenter,
               Case85CancellationToken *cancellationToken = nullptr);

    TestCaseId id() const override;
    QString displayName() const override;
    bool canStart() const override;
    TestCompletion start() override;
    void requestStop() override;

private:
    bool isCancellationRequested() const;
    void addLog(const QString &level, const QString &source, const QString &message);
    QList<double> frequencyPointsForBandwidth(int channelBWKHz) const;
    int totalRowCount() const;

    Analyzer4071 *m_analyzer;
    Generator1466 *m_generator;
    Case85RunConfig m_config;
    ITestEventSink *m_eventSink;
    ICase85ResultPresenter *m_presenter;
    Case85CancellationToken *m_cancellationToken;
};

#endif
