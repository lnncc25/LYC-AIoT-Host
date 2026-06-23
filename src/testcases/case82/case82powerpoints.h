#ifndef CASE82POWERPOINTS_H
#define CASE82POWERPOINTS_H

#include <QList>
#include <QString>

QList<double> parseCase82PowerPoints(const QString &text,
                                     bool *ok = nullptr,
                                     QString *errorMessage = nullptr);

#endif
