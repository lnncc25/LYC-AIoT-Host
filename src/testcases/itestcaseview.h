#ifndef ITESTCASEVIEW_H
#define ITESTCASEVIEW_H

#include <QString>

class ITestCaseView
{
public:
    virtual ~ITestCaseView() = default;

    virtual void showCaseSummary(const QString &title,
                                 const QString &frequency,
                                 const QString &bandwidth,
                                 int progress,
                                 const QString &progressFormat) = 0;
};

#endif
