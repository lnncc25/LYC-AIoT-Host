#ifndef LOGENTRY_H
#define LOGENTRY_H

#include <QDateTime>
#include <QMetaType>
#include <QString>

struct LogEntry {
    QDateTime timestamp;
    QString level;
    QString source;
    QString message;

    bool operator==(const LogEntry &other) const
    {
        return timestamp == other.timestamp
               && level == other.level
               && source == other.source
               && message == other.message;
    }
};

Q_DECLARE_METATYPE(LogEntry)

#endif // LOGENTRY_H
