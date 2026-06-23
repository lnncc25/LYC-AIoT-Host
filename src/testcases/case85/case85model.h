#ifndef CASE85MODEL_H
#define CASE85MODEL_H

#include <QList>
#include <QMap>
#include <QMetaType>
#include <QString>
#include <QStringList>

struct Case85OffsetConfig
{
    double offsetKHz = 0.0;
    double integrationBWKHz = 0.0;
    double limitDb = 0.0;
};

struct Case85BandwidthConfig
{
    int channelBWKHz = 0;
    QString iqFileName;
    QList<Case85OffsetConfig> offsets;
};

struct Case85RunConfig
{
    QString analyzerHost;
    quint16 analyzerPort = 5025;
    QString generatorHost;
    quint16 generatorPort = 5025;
    bool realInstrumentMode = false;
    double txPowerDbm = 0.0;
    QString screenshotRunDir;
    QMap<int, QString> frequencyTextByBandwidth;
    QList<Case85BandwidthConfig> bandwidthConfigs;

    bool isValid() const
    {
        return !bandwidthConfigs.isEmpty()
                && (!realInstrumentMode
                    || (!analyzerHost.isEmpty() && !generatorHost.isEmpty()));
    }
};

struct Case85RowDetail
{
    QString simImagePath;
    QString screenshotPath;
    QString statusText;
};

struct Case85RowResult
{
    int row = -1;
    int channelBWKHz = 0;
    double centerFreqMHz = 0.0;
    double offsetKHz = 0.0;
    double integrationBWKHz = 0.0;
    double limitDb = 0.0;
    bool measurementOk = false;
    bool pass = false;
    QString failureReason;
    QString testPointDesc;
    QString configDesc;
    QString measurementDesc;
    QString itemName;
    QString settingValue;
    QString standardValue;
    QString verdict;
    double mainPowerDbm = 0.0;
    double leftPowerDbm = 0.0;
    double rightPowerDbm = 0.0;
    double leftAclrDb = 0.0;
    double rightAclrDb = 0.0;
    Case85RowDetail detail;
};

struct Case85Result
{
    QList<Case85RowResult> rows;
    bool overallPass = false;
    bool hasValidResult = false;
    bool realInstrumentMode = false;
    int passCount = 0;
    int failCount = 0;
    QString screenshotRunDir;
};

Q_DECLARE_METATYPE(Case85OffsetConfig)
Q_DECLARE_METATYPE(Case85BandwidthConfig)
Q_DECLARE_METATYPE(Case85RunConfig)
Q_DECLARE_METATYPE(Case85RowDetail)
Q_DECLARE_METATYPE(Case85RowResult)
Q_DECLARE_METATYPE(Case85Result)

#endif
