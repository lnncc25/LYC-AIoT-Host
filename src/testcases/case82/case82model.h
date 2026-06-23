#ifndef CASE82MODEL_H
#define CASE82MODEL_H

#include <QList>
#include <QMetaType>
#include <QString>
#include <QtGlobal>
#include <QVector>

struct Case82RunConfig
{
    QString analyzerHost;
    quint16 analyzerPort = 5025;
    QString generatorHost;
    quint16 generatorPort = 5025;
    QList<double> powerPoints;
    double frequencyMHz = 0.0;
    QString bandwidth;
    double spanMHz = 1.0;

    bool isValid() const
    {
        return !analyzerHost.isEmpty()
                && !generatorHost.isEmpty()
                && !powerPoints.isEmpty()
                && frequencyMHz > 0.0;
    }
};

struct Case82Sample
{
    double timeSec = 0.0;
    double measuredPowerDbm = 0.0;
    double targetPowerDbm = 0.0;
};

struct Case82RowResult
{
    double targetPowerDbm = 0.0;
    bool generatorConfigured = false;
    bool analyzerConfigured = false;
    bool peakOk = false;
    double averagePowerDbm = 0.0;
    double averageFrequencyMHz = 0.0;
    double powerErrorDb = 0.0;
    double frequencyErrorKHz = 0.0;
    int sampleCount = 0;
    bool pass = false;
    QString failureReason;
};

struct Case82Result
{
    QString generatorIdn;
    QString analyzerIdn;
    QList<Case82RowResult> rows;
    QVector<double> trendTimeSec;
    QVector<double> trendMeasuredPowerDbm;
    QVector<double> trendTargetPowerDbm;
    bool analyzerLimitTriggered = false;
    bool overallPass = false;
    int passCount = 0;
    int failCount = 0;
    double maxAbsPowerErrorDb = 0.0;
};

Q_DECLARE_METATYPE(Case82Sample)
Q_DECLARE_METATYPE(Case82RunConfig)
Q_DECLARE_METATYPE(Case82RowResult)
Q_DECLARE_METATYPE(Case82Result)

#endif
