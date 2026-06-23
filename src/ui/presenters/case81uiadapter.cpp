#include "case81uiadapter.h"

Case81UiAdapter::Case81UiAdapter(SummaryHandler summaryHandler,
                                 LogHandler logHandler,
                                 ResultHandler resultHandler)
    : m_summaryHandler(std::move(summaryHandler))
    , m_logHandler(std::move(logHandler))
    , m_resultHandler(std::move(resultHandler))
{
}

void Case81UiAdapter::showCaseSummary(const QString &title,
                                      const QString &frequency,
                                      const QString &bandwidth,
                                      int progress,
                                      const QString &progressFormat)
{
    m_summaryHandler(title, frequency, bandwidth, progress, progressFormat);
}

void Case81UiAdapter::addTestLog(const QString &level,
                                 const QString &source,
                                 const QString &message)
{
    m_logHandler(level, source, message);
}

void Case81UiAdapter::presentCase81(const Case81Result &result)
{
    m_resultHandler(result);
}
