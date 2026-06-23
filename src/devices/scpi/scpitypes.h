#ifndef SCPITYPES_H
#define SCPITYPES_H

#include <QByteArray>
#include <QString>

class IScpiCancellation
{
public:
    virtual ~IScpiCancellation() = default;
    virtual bool isCancellationRequested() const = 0;
};

enum class ScpiRequestPriority {
    Normal,
    Safety
};

struct ScpiRequestOptions {
    ScpiRequestPriority priority = ScpiRequestPriority::Normal;
    const IScpiCancellation *cancellation = nullptr;
    bool abortActiveRequest = false;
};

enum class ScpiStatus {
    Success,
    NotConnected,
    WriteFailed,
    Timeout,
    ProtocolError,
    Cancelled
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
