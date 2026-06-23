#ifndef TESTTYPES_H
#define TESTTYPES_H

#include <QMetaType>
#include <QString>

enum class TestCaseId {
    Case81,
    Case82,
    Case83,
    Case84,
    Case85,
    Case86,
    Case87,
    Case88,
    Case89,
    Case810,
    Case811
};

enum class TestState {
    Idle,
    Validating,
    Preparing,
    Running,
    WaitingForUserInput,
    Pausing,
    Paused,
    Stopping,
    CleaningUp,
    Completed,
    ExecutionFailed,
    Cancelled
};

enum class CompletionReason {
    Completed,
    ExecutionFailed,
    Cancelled
};

enum class TestVerdict {
    Pass,
    Fail,
    Inconclusive,
    NotRun
};

struct TestCompletion {
    CompletionReason reason = CompletionReason::Completed;
    TestVerdict verdict = TestVerdict::NotRun;
    QString detail;

    bool operator==(const TestCompletion &other) const;
};

QString testCaseCode(TestCaseId id);
QString testStateName(TestState state);
QString completionReasonName(CompletionReason reason);
QString testVerdictName(TestVerdict verdict);

Q_DECLARE_METATYPE(TestCaseId)
Q_DECLARE_METATYPE(TestState)
Q_DECLARE_METATYPE(CompletionReason)
Q_DECLARE_METATYPE(TestVerdict)
Q_DECLARE_METATYPE(TestCompletion)

#endif // TESTTYPES_H
