#include "tcpscpitransport.h"

#include <QTcpSocket>

TcpScpiTransport::TcpScpiTransport(QObject *parent)
    : IScpiTransport(parent)
    , m_socket(new QTcpSocket(this))
{
    connect(m_socket, &QTcpSocket::connected, this, &IScpiTransport::connected);
    connect(m_socket, &QTcpSocket::readyRead, this, &IScpiTransport::readyRead);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(m_socket, &QTcpSocket::errorOccurred,
            this, &IScpiTransport::transportError);
#else
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),
            this, &IScpiTransport::transportError);
#endif
}

void TcpScpiTransport::connectToHost(const QString &host, quint16 port)
{
    m_socket->connectToHost(host, port);
}

void TcpScpiTransport::disconnectFromHost()
{
    m_socket->disconnectFromHost();
}

bool TcpScpiTransport::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

qint64 TcpScpiTransport::write(const QByteArray &data)
{
    return m_socket->write(data);
}

bool TcpScpiTransport::flush()
{
    return m_socket->flush();
}

qint64 TcpScpiTransport::bytesAvailable() const
{
    return m_socket->bytesAvailable();
}

QByteArray TcpScpiTransport::readAll()
{
    return m_socket->readAll();
}

bool TcpScpiTransport::waitForReadyRead(int timeoutMs)
{
    return m_socket->waitForReadyRead(timeoutMs);
}

bool TcpScpiTransport::waitForBytesWritten(int timeoutMs)
{
    return m_socket->waitForBytesWritten(timeoutMs);
}

QString TcpScpiTransport::errorString() const
{
    return m_socket->errorString();
}
