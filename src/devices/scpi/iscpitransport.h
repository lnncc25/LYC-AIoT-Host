#ifndef ISCPITRANSPORT_H
#define ISCPITRANSPORT_H

#include <QAbstractSocket>
#include <QByteArray>
#include <QObject>
#include <QString>

class IScpiTransport : public QObject
{
    Q_OBJECT

public:
    explicit IScpiTransport(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    ~IScpiTransport() override = default;

    virtual void connectToHost(const QString &host, quint16 port) = 0;
    virtual void disconnectFromHost() = 0;
    virtual bool isConnected() const = 0;
    virtual bool waitForConnected(int timeoutMs) = 0;
    virtual qint64 write(const QByteArray &data) = 0;
    virtual bool flush() = 0;
    virtual qint64 bytesAvailable() const = 0;
    virtual QByteArray readAll() = 0;
    virtual bool waitForReadyRead(int timeoutMs) = 0;
    virtual bool waitForBytesWritten(int timeoutMs) = 0;
    virtual QString errorString() const = 0;

signals:
    void connected();
    void readyRead();
    void transportError(QAbstractSocket::SocketError error);
};

#endif
