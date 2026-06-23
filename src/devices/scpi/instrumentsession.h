#ifndef INSTRUMENTSESSION_H
#define INSTRUMENTSESSION_H

#include "scpitypes.h"

#include <QAbstractSocket>
#include <QObject>
#include <atomic>

class IScpiTransport;
class QMutex;
class QWaitCondition;

class InstrumentSession : public QObject
{
    Q_OBJECT

public:
    explicit InstrumentSession(IScpiTransport *transport, QObject *parent = nullptr);
    ~InstrumentSession() override;

    void connectToHost(const QString &host, quint16 port);
    void disconnectFromHost();
    bool isConnected() const;
    bool waitForConnected(int timeoutMs);
    bool isQuerying() const;
    QString errorString() const;
    QByteArray takeAvailableData();
    bool waitForBytesWritten(int timeoutMs);
    void setDefaultCancellation(const IScpiCancellation *cancellation);
    const IScpiCancellation *defaultCancellation() const;
    bool abortActiveRequest();

    ScpiWriteResult send(const QString &command);
    ScpiWriteResult send(const QString &command,
                         const ScpiRequestOptions &options);
    ScpiReply query(const QString &command, int timeoutMs = 2000);
    ScpiReply query(const QString &command,
                    int timeoutMs,
                    const ScpiRequestOptions &options);
    BinaryBlockReply queryBinaryBlock(const QString &command, int timeoutMs = 5000);
    BinaryBlockReply queryBinaryBlock(const QString &command,
                                      int timeoutMs,
                                      const ScpiRequestOptions &options);

signals:
    void connected();
    void readyRead();
    void sessionError(QAbstractSocket::SocketError error);
    void commandSent(const QString &command);
    void commandSendFailed(const QString &error);

private:
    enum class RequestKind {
        Send,
        Query,
        BinaryQuery
    };

    struct PendingRequest {
        quint64 id = 0;
        RequestKind kind = RequestKind::Send;
        QString command;
        int timeoutMs = 0;
        ScpiRequestOptions options;
        bool completed = false;
        ScpiWriteResult writeResult;
        ScpiReply reply;
        BinaryBlockReply binaryReply;
    };

    ScpiWriteResult performSend(const QString &command,
                                const ScpiRequestOptions &options);
    ScpiReply performQuery(const QString &command,
                           int timeoutMs,
                           const ScpiRequestOptions &options);
    BinaryBlockReply performBinaryQuery(const QString &command,
                                        int timeoutMs,
                                        const ScpiRequestOptions &options);
    void completePendingRequest(PendingRequest &request);
    void finishActiveRequestLocked(PendingRequest *request);
    PendingRequest *nextPendingRequestLocked() const;
    void removePendingRequestLocked(PendingRequest *request);
    const IScpiCancellation *effectiveCancellation(
        const ScpiRequestOptions &options) const;
    bool requestShouldStop(const ScpiRequestOptions &options) const;
    QString requestStopReason(const ScpiRequestOptions &options) const;
    void clearPendingInput();

    IScpiTransport *m_transport;
    bool m_querying;
    const IScpiCancellation *m_defaultCancellation;
    QMutex *m_queueMutex;
    QWaitCondition *m_queueChanged;
    QList<PendingRequest *> *m_normalQueue;
    QList<PendingRequest *> *m_safetyQueue;
    PendingRequest *m_activeRequest;
    quint64 m_nextRequestId;
    std::atomic_bool m_abortRequested;
};

#endif
