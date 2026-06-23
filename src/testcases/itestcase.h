#ifndef ITESTCASE_H
#define ITESTCASE_H

#include "testtypes.h"

#include <QString>

class ITestCase
{
public:
    virtual ~ITestCase() = default;

    virtual TestCaseId id() const = 0;
    virtual QString displayName() const = 0;
    virtual bool canStart() const = 0;
    virtual TestCompletion start() = 0;
    virtual void requestStop() = 0;
};

#endif
