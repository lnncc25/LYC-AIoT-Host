#ifndef TESTCASE82_H
#define TESTCASE82_H

#include "case82model.h"
#include "itestcase.h"

class Analyzer4071;
class Case82CancellationToken;
class Generator1466;
class ICase82ResultPresenter;
class ITestEventSink;

class TestCase82 : public ITestCase
{
public:
    TestCase82(Analyzer4071 *analyzer,
               Generator1466 *generator,
               const Case82RunConfig &config,
               ITestEventSink *eventSink,
               ICase82ResultPresenter *presenter,
               Case82CancellationToken *cancellationToken = nullptr);

    TestCaseId id() const override;
    QString displayName() const override;
    bool canStart() const override;
    TestCompletion start() override;
    void requestStop() override;

private:
    bool isCancellationRequested() const;
    bool cancellableSleep(int milliseconds) const;
    void addLog(const QString &level, const QString &source, const QString &message);

    Analyzer4071 *m_analyzer;
    Generator1466 *m_generator;
    Case82RunConfig m_config;
    ITestEventSink *m_eventSink;
    ICase82ResultPresenter *m_presenter;
    Case82CancellationToken *m_cancellationToken;
};

#endif
