#include "fakescpitransport.h"

FakeScpiTransport::FakeScpiTransport(QObject *parent)
    : IScpiTransport(parent)
    , m_connected(false)
{
}

void FakeScpiTransport::connectToHost(const QString &, quint16)
{
    m_connected = true;
    emit connected();
}

void FakeScpiTransport::disconnectFromHost()
{
    m_connected = false;
}

bool FakeScpiTransport::isConnected() const
{
    return m_connected;
}

qint64 FakeScpiTransport::write(const QByteArray &data)
{
    if (!m_connected) {
        return -1;
    }

    m_writes.append(data);
    const QString command = QString::fromUtf8(data).trimmed();
    m_commands.append(command);

    QList<QByteArray> &responses = m_responses[command];
    if (!responses.isEmpty()) {
        m_pendingData.append(responses.takeFirst());
    }
    return data.size();
}

bool FakeScpiTransport::flush()
{
    return true;
}

qint64 FakeScpiTransport::bytesAvailable() const
{
    return m_pendingData.size();
}

QByteArray FakeScpiTransport::readAll()
{
    const QByteArray data = m_pendingData;
    m_pendingData.clear();
    return data;
}

bool FakeScpiTransport::waitForReadyRead(int)
{
    return !m_pendingData.isEmpty();
}

bool FakeScpiTransport::waitForBytesWritten(int)
{
    return m_connected;
}

QString FakeScpiTransport::errorString() const
{
    return m_connected ? QString() : QStringLiteral("fake transport disconnected");
}

void FakeScpiTransport::setConnected(bool connected)
{
    m_connected = connected;
}

void FakeScpiTransport::enqueueResponse(const QString &command, const QByteArray &response)
{
    m_responses[command].append(response);
}

QStringList FakeScpiTransport::commandTrace() const
{
    return m_commands;
}

QList<QByteArray> FakeScpiTransport::writeTrace() const
{
    return m_writes;
}
