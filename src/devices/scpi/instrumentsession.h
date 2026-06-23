#ifndef INSTRUMENTSESSION_H
#define INSTRUMENTSESSION_H

#include "scpitypes.h"

#include <QAbstractSocket>
#include <QObject>

class IScpiTransport;

class InstrumentSession : public QObject
{
    Q_OBJECT

public:
    explicit InstrumentSession(IScpiTransport *transport, QObject *parent = nullptr);

    void connectToHost(const QString &host, quint16 port);
    void disconnectFromHost();
    bool isConnected() const;
    bool isQuerying() const;
    QString errorString() const;
    QByteArray takeAvailableData();
    bool waitForBytesWritten(int timeoutMs);

    ScpiWriteResult send(const QString &command);
    ScpiReply query(const QString &command, int timeoutMs = 2000);
    BinaryBlockReply queryBinaryBlock(const QString &command, int timeoutMs = 5000);

signals:
    void connected();
    void readyRead();
    void sessionError(QAbstractSocket::SocketError error);
    void commandSent(const QString &command);
    void commandSendFailed(const QString &error);

private:
    void clearPendingInput();

    IScpiTransport *m_transport;
    bool m_querying;
};

#endif
