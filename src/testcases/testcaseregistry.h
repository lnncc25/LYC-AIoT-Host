#ifndef TESTCASEREGISTRY_H
#define TESTCASEREGISTRY_H

#include "testtypes.h"

#include <map>
#include <memory>

class ITestCase;

class TestCaseRegistry
{
public:
    void registerCase(std::unique_ptr<ITestCase> testCase);
    ITestCase *find(TestCaseId id) const;

private:
    std::map<TestCaseId, std::unique_ptr<ITestCase>> m_cases;
};

#endif
