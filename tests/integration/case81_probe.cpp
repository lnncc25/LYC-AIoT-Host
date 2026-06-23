#include "case81model.h"
#include "case81runconfig.h"
#include "case81runcontroller.h"
#include "testtypes.h"

#include <QCoreApplication>
#include <QTextStream>
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList arguments = app.arguments();
    if (arguments.size() != 5) {
        QTextStream(stderr)
            << "Usage: case81_probe <analyzer-host> <analyzer-port> "
               "<generator-host> <generator-port>\n";
        return 2;
    }

    bool analyzerPortOk = false;
    bool generatorPortOk = false;
    const uint analyzerPort = arguments.at(2).toUInt(&analyzerPortOk);
    const uint generatorPort = arguments.at(4).toUInt(&generatorPortOk);
    if (!analyzerPortOk || analyzerPort > 65535
        || !generatorPortOk || generatorPort > 65535) {
        QTextStream(stderr) << "Invalid port\n";
        return 2;
    }

    Case81RunConfig config;
    config.analyzerHost = arguments.at(1);
    config.analyzerPort = static_cast<quint16>(analyzerPort);
    config.generatorHost = arguments.at(3);
    config.generatorPort = static_cast<quint16>(generatorPort);

    Case81RunController controller;
    QStringList states;
    QStringList eventOrder;
    Case81Result result;
    bool resultReceived = false;
    bool cleanupSafe = false;
    int finishCount = 0;
    int exitCode = 5;

    QObject::connect(&controller, &Case81RunController::stateChanged,
                     [&states](TestState state) {
        states.append(testStateName(state));
    });
    QObject::connect(&controller, &Case81RunController::resultReady,
                     [&result, &resultReceived](const Case81Result &value) {
        result = value;
        resultReceived = true;
    });
    QObject::connect(&controller, &Case81RunController::cleanupCompleted,
                     [&cleanupSafe, &eventOrder](bool safe) {
        cleanupSafe = safe;
        eventOrder.append(QStringLiteral("cleanup"));
    });
    QObject::connect(&controller, &Case81RunController::finished,
                     [&](const TestCompletion &completion) {
        ++finishCount;
        eventOrder.append(QStringLiteral("finished"));
        QTextStream output(stdout);
        output << "STATES=" << states.join("->") << '\n'
               << "EVENTS=" << eventOrder.join("->") << '\n'
               << "FINISH_COUNT=" << finishCount << '\n'
               << "COMPLETION=" << completionReasonName(completion.reason) << '\n'
               << "VERDICT=" << testVerdictName(completion.verdict) << '\n'
               << "CLEANUP_SAFE=" << (cleanupSafe ? "true" : "false") << '\n'
               << "ANALYZER_IDN=" << result.analyzerIdn << '\n'
               << "GENERATOR_IDN=" << result.generatorIdn << '\n'
               << "ANALYZER_ERROR=" << result.analyzerError << '\n'
               << "GENERATOR_ERROR=" << result.generatorError << '\n';

        const QStringList expectedStates = {
            "Validating", "Preparing", "Running", "CleaningUp", "Completed"
        };
        exitCode = resultReceived
                && states == expectedStates
                && eventOrder == QStringList({"cleanup", "finished"})
                && finishCount == 1
                && cleanupSafe
                && completion.reason == CompletionReason::Completed
                && completion.verdict == TestVerdict::Pass
                ? 0 : 4;
        app.quit();
    });

    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, [&]() {
        controller.requestStop();
        exitCode = 6;
        app.quit();
    });
    timeout.start(15000);

    if (!controller.start(config)) {
        QTextStream(stderr) << "Could not start case 8.1\n";
        return 3;
    }
    app.exec();
    controller.waitForFinished(8000);
    return exitCode;
}
