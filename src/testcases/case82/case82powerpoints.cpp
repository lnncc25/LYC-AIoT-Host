#include "case82powerpoints.h"

#include "case82constants.h"

#include <QRegularExpression>
#include <QtGlobal>
#include <cmath>

QList<double> parseCase82PowerPoints(const QString &input,
                                     bool *ok,
                                     QString *errorMessage)
{
    if (ok) {
        *ok = false;
    }
    if (errorMessage) {
        errorMessage->clear();
    }

    QString text = input.trimmed();
    if (text.isEmpty()) {
        text = QStringLiteral("0,3,7.5,12,15");
    }

    text.replace(QChar(0xff0c), ',');
    text.replace(QChar(0x3001), ',');
    text.replace(';', ',');
    text.replace(QChar(0xff1b), ',');

    const QStringList tokens = text.split(QRegularExpression("[,\\s]+"),
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
                                          Qt::SkipEmptyParts
#else
                                          QString::SkipEmptyParts
#endif
                                          );
    if (tokens.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("8.2 功率点为空，请输入 0~15 dBm 范围内的点位");
        }
        return {};
    }

    QList<double> points;
    for (const QString &token : tokens) {
        bool valueOk = false;
        const double value = token.toDouble(&valueOk);
        if (!valueOk || !std::isfinite(value)) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("8.2 功率点包含非法数值: %1").arg(token);
            }
            return {};
        }
        if (value < Case82Constants::OutputPowerMinDbm
            || value > Case82Constants::OutputPowerMaxDbm) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("8.2 功率点 %1 dBm 超出当前允许范围 %2~%3 dBm")
                        .arg(value, 0, 'f', 2)
                        .arg(Case82Constants::OutputPowerMinDbm, 0, 'f', 0)
                        .arg(Case82Constants::OutputPowerMaxDbm, 0, 'f', 0);
            }
            return {};
        }
        points.append(value);
    }

    QList<double> uniquePoints;
    for (double point : points) {
        bool duplicate = false;
        for (double existing : uniquePoints) {
            if (qAbs(existing - point) < 0.0001) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            uniquePoints.append(point);
        }
    }

    if (uniquePoints.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("8.2 功率点解析结果为空");
        }
        return {};
    }

    if (ok) {
        *ok = true;
    }
    return uniquePoints;
}
