#ifndef CASE81UIADAPTER_H
#define CASE81UIADAPTER_H

#include "icase81resultpresenter.h"
#include "itestcaseview.h"
#include "itesteventsink.h"

#include <functional>

class Case81UiAdapter : public ITestCaseView,
                        public ITestEventSink,
                        public ICase81ResultPresenter
{
public:
    using SummaryHandler = std::function<void(const QString &, const QString &,
                                              const QString &, int, const QString &)>;
    using LogHandler = std::function<void(const QString &, const QString &, const QString &)>;
    using ResultHandler = std::function<void(const Case81Result &)>;

    Case81UiAdapter(SummaryHandler summaryHandler,
                    LogHandler logHandler,
                    ResultHandler resultHandler);

    void showCaseSummary(const QString &title,
                         const QString &frequency,
                         const QString &bandwidth,
                         int progress,
                         const QString &progressFormat) override;
    void addTestLog(const QString &level,
                    const QString &source,
                    const QString &message) override;
    void presentCase81(const Case81Result &result) override;

private:
    SummaryHandler m_summaryHandler;
    LogHandler m_logHandler;
    ResultHandler m_resultHandler;
};

#endif
