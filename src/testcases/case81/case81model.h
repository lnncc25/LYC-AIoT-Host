#ifndef CASE81MODEL_H
#define CASE81MODEL_H

#include <QString>

struct Case81Result {
    bool dualInstrumentMode = false;
    QString analyzerIdn;
    QString generatorIdn;
    QString analyzerError;
    QString generatorError;
    bool analyzerIdentified = false;
    bool generatorIdentified = false;
    bool voltageAvailable = false;
    double voltage = 0.0;
    bool overallPass = false;
};

#endif
