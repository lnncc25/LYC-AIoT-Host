#include "case85model.h"
#include "case85runcontroller.h"
#include "outputpaths.h"
#include "testtypes.h"

#include <QCoreApplication>
#include <QTextStream>
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList arguments = app.arguments();
    if (arguments.size() < 7) {
        QTextStream(stderr)
            << "Usage: case85_probe <analyzer-host> <analyzer-port> "
               "<generator-host> <generator-port> <tx-power-dbm> <freq-list>\n";
        return 2;
    }

    bool analyzerPortOk = false;
    bool generatorPortOk = false;
    bool powerOk = false;
    const uint analyzerPort = arguments.at(2).toUInt(&analyzerPortOk);
    const uint generatorPort = arguments.at(4).toUInt(&generatorPortOk);
    const double txPowerDbm = arguments.at(5).toDouble(&powerOk);
    if (!analyzerPortOk || analyzerPort > 65535
        || !generatorPortOk || generatorPort > 65535
        || !powerOk) {
        QTextStream(stderr) << "Invalid argument\n";
        return 2;
    }

    Case85RunConfig config;
    config.realInstrumentMode = true;
    config.analyzerHost = arguments.at(1);
    config.analyzerPort = static_cast<quint16>(analyzerPort);
    config.generatorHost = arguments.at(3);
    config.generatorPort = static_cast<quint16>(generatorPort);
    config.txPowerDbm = txPowerDbm;
    config.frequencyTextByBandwidth.insert(200, arguments.at(6));
    config.bandwidthConfigs = {
        {200, "AIoT_TxSignal_iq_200kHz.bin", {{300, 180, 40.8}, {500, 180, 45.8}}}
    };
    config.screenshotRunDir = OutputPaths::screenshotRunDirectory(
        "8.5_ACLR_probe", OutputPaths::batchTimestamp());

    Case85RunController controller;
    QStringList states;
    QStringList eventOrder;
    QStringList logs;
    Case85Result result;
    bool cleanupSafe = false;
    int finishCount = 0;
    int exitCode = 5;

    QObject::connect(&controller, &Case85RunController::stateChanged,
                     [&states](TestState state) {
        states.append(testStateName(state));
    });
    QObject::connect(&controller, &Case85RunController::resultReady,
                     [&result](const Case85Result &value) {
        result = value;
    });
    QObject::connect(&controller, &Case85RunController::logReady,
                     [&logs](const QString &level,
                             const QString &source,
                             const QString &message) {
        logs.append(QStringLiteral("%1|%2|%3").arg(level, source, message));
    });
    QObject::connect(&controller, &Case85RunController::cleanupCompleted,
                     [&cleanupSafe, &eventOrder](bool safe) {
        cleanupSafe = safe;
        eventOrder.append(QStringLiteral("cleanup"));
    });
    QObject::connect(&controller, &Case85RunController::finished,
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
               << "ROWS=" << result.rows.size() << '\n'
               << "PASS_COUNT=" << result.passCount << '\n'
               << "FAIL_COUNT=" << result.failCount << '\n'
               << "SCREENSHOT_DIR=" << result.screenshotRunDir << '\n'
               << "FIRST_ROW_VERDICT="
               << (result.rows.isEmpty() ? QString() : result.rows.first().verdict) << '\n';
        for (int i = 0; i < result.rows.size(); ++i) {
            const Case85RowResult &row = result.rows.at(i);
            output << "ROW[" << i << "].ITEM=" << row.itemName << '\n'
                   << "ROW[" << i << "].VERDICT=" << row.verdict << '\n'
                   << "ROW[" << i << "].LEFT_ACLR_DB=" << QString::number(row.leftAclrDb, 'f', 2) << '\n'
                   << "ROW[" << i << "].RIGHT_ACLR_DB=" << QString::number(row.rightAclrDb, 'f', 2) << '\n'
                   << "ROW[" << i << "].SCREENSHOT=" << row.detail.screenshotPath << '\n';
        }
        for (int i = 0; i < logs.size(); ++i) {
            output << "LOG[" << i << "]=" << logs.at(i) << '\n';
        }
        exitCode = finishCount == 1
                && eventOrder == QStringList({"cleanup", "finished"})
                && cleanupSafe
                && !result.rows.isEmpty()
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
    timeout.start(60000);

    if (!controller.start(config)) {
        QTextStream(stderr) << "Could not start case 8.5\n";
        return 3;
    }
    app.exec();
    controller.waitForFinished(8000);
    return exitCode;
}
