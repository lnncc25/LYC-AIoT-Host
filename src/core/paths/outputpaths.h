#ifndef OUTPUTPATHS_H
#define OUTPUTPATHS_H

#include <QDateTime>
#include <QString>

namespace OutputPaths {

QString rootDirectory();
QString path(const QString &name);
QString screenshotRunDirectory(const QString &caseDirectory, const QString &batchTimestamp);
QString batchTimestamp(const QDateTime &dateTime = QDateTime::currentDateTime());
QString fileTimestamp(const QDateTime &dateTime = QDateTime::currentDateTime());
QString timestampedScreenshotName(const QString &baseName,
                                  const QDateTime &dateTime = QDateTime::currentDateTime());
QString sanitizeFileToken(QString value);

}

#endif // OUTPUTPATHS_H
