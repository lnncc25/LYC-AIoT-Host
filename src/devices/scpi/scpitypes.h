#ifndef SCPITYPES_H
#define SCPITYPES_H

#include <QByteArray>
#include <QString>

enum class ScpiStatus {
    Success,
    NotConnected,
    WriteFailed,
    Timeout,
    ProtocolError
};

struct ScpiWriteResult {
    ScpiStatus status = ScpiStatus::Success;
    QString error;

    bool isSuccess() const
    {
        return status == ScpiStatus::Success;
    }
};

struct ScpiReply {
    ScpiStatus status = ScpiStatus::Success;
    QString text;
    QString error;

    bool isSuccess() const
    {
        return status == ScpiStatus::Success;
    }
};

struct BinaryBlockReply {
    ScpiStatus status = ScpiStatus::Success;
    QByteArray payload;
    int receivedBytes = 0;
    QString error;

    bool isSuccess() const
    {
        return status == ScpiStatus::Success;
    }
};

#endif
