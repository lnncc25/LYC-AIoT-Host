#ifndef ITESTEVENTSINK_H
#define ITESTEVENTSINK_H

#include <QString>

class ITestEventSink
{
public:
    virtual ~ITestEventSink() = default;

    virtual void addTestLog(const QString &level,
                            const QString &source,
                            const QString &message) = 0;
};

#endif
