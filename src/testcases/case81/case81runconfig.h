#ifndef CASE81RUNCONFIG_H
#define CASE81RUNCONFIG_H

#include <QString>
#include <QtGlobal>

struct Case81RunConfig {
    QString analyzerHost;
    quint16 analyzerPort = 5025;
    QString generatorHost;
    quint16 generatorPort = 5025;

    bool hasAnalyzer() const
    {
        return !analyzerHost.trimmed().isEmpty();
    }

    bool hasGenerator() const
    {
        return !generatorHost.trimmed().isEmpty();
    }
};

#endif
