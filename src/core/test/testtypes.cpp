#include "testtypes.h"

bool TestCompletion::operator==(const TestCompletion &other) const
{
    return reason == other.reason
           && verdict == other.verdict
           && detail == other.detail;
}

QString testCaseCode(TestCaseId id)
{
    switch (id) {
    case TestCaseId::Case81: return "8.1";
    case TestCaseId::Case82: return "8.2";
    case TestCaseId::Case83: return "8.3";
    case TestCaseId::Case84: return "8.4";
    case TestCaseId::Case85: return "8.5";
    case TestCaseId::Case86: return "8.6";
    case TestCaseId::Case87: return "8.7";
    case TestCaseId::Case88: return "8.8";
    case TestCaseId::Case89: return "8.9";
    case TestCaseId::Case810: return "8.10";
    case TestCaseId::Case811: return "8.11";
    }
    return QString();
}

QString testStateName(TestState state)
{
    switch (state) {
    case TestState::Idle: return "Idle";
    case TestState::Validating: return "Validating";
    case TestState::Preparing: return "Preparing";
    case TestState::Running: return "Running";
    case TestState::WaitingForUserInput: return "WaitingForUserInput";
    case TestState::Pausing: return "Pausing";
    case TestState::Paused: return "Paused";
    case TestState::Stopping: return "Stopping";
    case TestState::CleaningUp: return "CleaningUp";
    case TestState::Completed: return "Completed";
    case TestState::ExecutionFailed: return "ExecutionFailed";
    case TestState::Cancelled: return "Cancelled";
    }
    return QString();
}

QString completionReasonName(CompletionReason reason)
{
    switch (reason) {
    case CompletionReason::Completed: return "Completed";
    case CompletionReason::ExecutionFailed: return "ExecutionFailed";
    case CompletionReason::Cancelled: return "Cancelled";
    }
    return QString();
}

QString testVerdictName(TestVerdict verdict)
{
    switch (verdict) {
    case TestVerdict::Pass: return "PASS";
    case TestVerdict::Fail: return "FAIL";
    case TestVerdict::Inconclusive: return "INCONCLUSIVE";
    case TestVerdict::NotRun: return "NOT_RUN";
    }
    return QString();
}
