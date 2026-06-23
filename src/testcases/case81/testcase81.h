#ifndef TESTCASE81_H
#define TESTCASE81_H

#include "itestcase.h"

class Analyzer4071;
class Case81CancellationToken;
class Generator1466;
class ICase81ResultPresenter;
class ITestCaseView;
class ITestEventSink;
struct ScpiReply;

class TestCase81 : public ITestCase
{
public:
    TestCase81(Analyzer4071 *analyzer,
               Generator1466 *generator,
               ITestCaseView *view,
               ITestEventSink *eventSink,
               ICase81ResultPresenter *resultPresenter,
               Case81CancellationToken *cancellationToken = nullptr);

    TestCaseId id() const override;
    QString displayName() const override;
    bool canStart() const override;
    TestCompletion start() override;
    void requestStop() override;

private:
    QString responseText(const ScpiReply &reply,
                         const QString &source,
                         const QString &command,
                         bool receivedStyle = false) const;
    bool cancellationRequested() const;
    TestCompletion cancelledCompletion() const;

    Analyzer4071 *m_analyzer;
    Generator1466 *m_generator;
    ITestCaseView *m_view;
    ITestEventSink *m_eventSink;
    ICase81ResultPresenter *m_resultPresenter;
    Case81CancellationToken *m_cancellationToken;
};

#endif
