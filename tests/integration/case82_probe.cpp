#include "case82model.h"
#include "case82runcontroller.h"
#include "testtypes.h"

#include <QCoreApplication>
#include <QTextStream>
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList arguments = app.arguments();
    if (arguments.size() < 8) {
        QTextStream(stderr)
            << "Usage: case82_probe <analyzer-host> <analyzer-port> "
               "<generator-host> <generator-port> <freq-mhz> <bandwidth-khz> <power-dbm>\n";
        return 2;
    }

    bool analyzerPortOk = false;
    bool generatorPortOk = false;
    bool frequencyOk = false;
    bool bandwidthOk = false;
    bool powerOk = false;
    const uint analyzerPort = arguments.at(2).toUInt(&analyzerPortOk);
    const uint generatorPort = arguments.at(4).toUInt(&generatorPortOk);
    const double frequencyMHz = arguments.at(5).toDouble(&frequencyOk);
    const double bandwidthKHz = arguments.at(6).toDouble(&bandwidthOk);
    const double powerDbm = arguments.at(7).toDouble(&powerOk);
    if (!analyzerPortOk || analyzerPort > 65535
        || !generatorPortOk || generatorPort > 65535
        || !frequencyOk || !bandwidthOk || !powerOk) {
        QTextStream(stderr) << "Invalid argument\n";
        return 2;
    }

    Case82RunConfig config;
    config.analyzerHost = arguments.at(1);
    config.analyzerPort = static_cast<quint16>(analyzerPort);
    config.generatorHost = arguments.at(3);
    config.generatorPort = static_cast<quint16>(generatorPort);
    config.frequencyMHz = frequencyMHz;
    config.bandwidth = QString::number(bandwidthKHz, 'f', 0);
    config.spanMHz = qMax(1.0, bandwidthKHz / 1000.0 * 4.0);
    config.powerPoints = {powerDbm};

    Case82RunController controller;
    QStringList states;
    QStringList eventOrder;
    Case82Result result;
    bool cleanupSafe = false;
    int finishCount = 0;
    int sampleCount = 0;
    int exitCode = 5;

    QObject::connect(&controller, &Case82RunController::stateChanged,
                     [&states](TestState state) {
        states.append(testStateName(state));
    });
    QObject::connect(&controller, &Case82RunController::sampleReady,
                     [&sampleCount](const Case82Sample &) {
        ++sampleCount;
    });
    QObject::connect(&controller, &Case82RunController::resultReady,
                     [&result](const Case82Result &value) {
        result = value;
    });
    QObject::connect(&controller, &Case82RunController::cleanupCompleted,
                     [&cleanupSafe, &eventOrder](bool safe) {
        cleanupSafe = safe;
        eventOrder.append(QStringLiteral("cleanup"));
    });
    QObject::connect(&controller, &Case82RunController::finished,
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
               << "SAMPLES=" << sampleCount << '\n'
               << "ROWS=" << result.rows.size() << '\n'
               << "PASS_COUNT=" << result.passCount << '\n'
               << "FAIL_COUNT=" << result.failCount << '\n'
               << "FIRST_ROW_REASON="
               << (result.rows.isEmpty() ? QString() : result.rows.first().failureReason) << '\n'
               << "ANALYZER_IDN=" << result.analyzerIdn << '\n'
               << "GENERATOR_IDN=" << result.generatorIdn << '\n';

        exitCode = states == QStringList({"Validating", "Preparing", "Running", "CleaningUp", "Completed"})
                && eventOrder == QStringList({"cleanup", "finished"})
                && finishCount == 1
                && cleanupSafe
                && result.rows.size() == 1
                && sampleCount == 10
                && completion.reason == CompletionReason::Completed
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
    timeout.start(30000);

    if (!controller.start(config)) {
        QTextStream(stderr) << "Could not start case 8.2\n";
        return 3;
    }
    app.exec();
    controller.waitForFinished(8000);
    return exitCode;
}
