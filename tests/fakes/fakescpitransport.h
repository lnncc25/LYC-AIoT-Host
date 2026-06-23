#ifndef FAKESCPITRANSPORT_H
#define FAKESCPITRANSPORT_H

#include "iscpitransport.h"

#include <QHash>
#include <QList>
#include <QStringList>

class FakeScpiTransport : public IScpiTransport
{
public:
    explicit FakeScpiTransport(QObject *parent = nullptr);

    void connectToHost(const QString &host, quint16 port) override;
    void disconnectFromHost() override;
    bool isConnected() const override;
    qint64 write(const QByteArray &data) override;
    bool flush() override;
    qint64 bytesAvailable() const override;
    QByteArray readAll() override;
    bool waitForReadyRead(int timeoutMs) override;
    bool waitForBytesWritten(int timeoutMs) override;
    QString errorString() const override;

    void setConnected(bool connected);
    void enqueueResponse(const QString &command, const QByteArray &response);
    QStringList commandTrace() const;
    QList<QByteArray> writeTrace() const;

private:
    bool m_connected;
    QByteArray m_pendingData;
    QHash<QString, QList<QByteArray>> m_responses;
    QStringList m_commands;
    QList<QByteArray> m_writes;
};

#endif
