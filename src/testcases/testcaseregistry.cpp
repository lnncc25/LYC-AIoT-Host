#include "testcaseregistry.h"

#include "itestcase.h"

void TestCaseRegistry::registerCase(std::unique_ptr<ITestCase> testCase)
{
    if (!testCase) {
        return;
    }
    m_cases[testCase->id()] = std::move(testCase);
}

ITestCase *TestCaseRegistry::find(TestCaseId id) const
{
    const auto it = m_cases.find(id);
    return it == m_cases.end() ? nullptr : it->second.get();
}
