#ifndef TCPSCPITRANSPORT_H
#define TCPSCPITRANSPORT_H

#include "iscpitransport.h"

class QTcpSocket;

class TcpScpiTransport : public IScpiTransport
{
    Q_OBJECT

public:
    explicit TcpScpiTransport(QObject *parent = nullptr);

    void connectToHost(const QString &host, quint16 port) override;
    void disconnectFromHost() override;
    bool isConnected() const override;
    bool waitForConnected(int timeoutMs) override;
    qint64 write(const QByteArray &data) override;
    bool flush() override;
    qint64 bytesAvailable() const override;
    QByteArray readAll() override;
    bool waitForReadyRead(int timeoutMs) override;
    bool waitForBytesWritten(int timeoutMs) override;
    QString errorString() const override;

private:
    QTcpSocket *m_socket;
};

#endif
