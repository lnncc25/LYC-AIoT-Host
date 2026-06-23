#include "outputpaths.h"

#include <QDir>

namespace OutputPaths {

QString rootDirectory()
{
#ifdef APP_OUTPUT_ROOT
    const QString configuredRoot = QString::fromUtf8(APP_OUTPUT_ROOT);
    if (!configuredRoot.isEmpty()) {
        return QDir(configuredRoot).absolutePath();
    }
#endif
    return QDir::currentPath();
}

QString path(const QString &name)
{
    return QDir(rootDirectory()).filePath(name);
}

QString screenshotRunDirectory(const QString &caseDirectory, const QString &batchTimestamp)
{
    return QDir(path("screenshots")).filePath(
        QDir(caseDirectory).filePath(batchTimestamp));
}

QString batchTimestamp(const QDateTime &dateTime)
{
    return dateTime.toString("yyyyMMdd_HHmmss");
}

QString fileTimestamp(const QDateTime &dateTime)
{
    return dateTime.toString("HHmmss");
}

QString timestampedScreenshotName(const QString &baseName, const QDateTime &dateTime)
{
    return QString("%1_%2.png").arg(baseName, fileTimestamp(dateTime));
}

QString sanitizeFileToken(QString value)
{
    value.replace(' ', '_');
    value.replace(',', '_');
    value.replace('+', 'p');
    value.replace('-', 'm');
    value.replace('/', '_');
    return value;
}

}
