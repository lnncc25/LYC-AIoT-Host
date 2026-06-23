#include "instrumentsession.h"

#include "iscpitransport.h"

#include <QCoreApplication>
#include <QElapsedTimer>

namespace {
class QueryScope
{
public:
    explicit QueryScope(bool &querying)
        : m_querying(querying)
    {
        m_querying = true;
    }

    ~QueryScope()
    {
        m_querying = false;
    }

private:
    bool &m_querying;
};
}

InstrumentSession::InstrumentSession(IScpiTransport *transport, QObject *parent)
    : QObject(parent)
    , m_transport(transport)
    , m_querying(false)
{
    Q_ASSERT(m_transport);
    if (!m_transport->parent()) {
        m_transport->setParent(this);
    }
    connect(m_transport, &IScpiTransport::connected,
            this, &InstrumentSession::connected);
    connect(m_transport, &IScpiTransport::readyRead,
            this, &InstrumentSession::readyRead);
    connect(m_transport, &IScpiTransport::transportError,
            this, &InstrumentSession::sessionError);
}

void InstrumentSession::connectToHost(const QString &host, quint16 port)
{
    m_transport->connectToHost(host, port);
}

void InstrumentSession::disconnectFromHost()
{
    m_transport->disconnectFromHost();
}

bool InstrumentSession::isConnected() const
{
    return m_transport->isConnected();
}

bool InstrumentSession::isQuerying() const
{
    return m_querying;
}

QString InstrumentSession::errorString() const
{
    return m_transport->errorString();
}

QByteArray InstrumentSession::takeAvailableData()
{
    return m_transport->readAll();
}

bool InstrumentSession::waitForBytesWritten(int timeoutMs)
{
    return m_transport->waitForBytesWritten(timeoutMs);
}

ScpiWriteResult InstrumentSession::send(const QString &command)
{
    if (!isConnected()) {
        return {ScpiStatus::NotConnected, QStringLiteral("not connected")};
    }

    QByteArray data = command.toUtf8();
    if (!data.endsWith('\n')) {
        data.append('\n');
    }

    if (m_transport->write(data) == -1) {
        const QString error = m_transport->errorString();
        emit commandSendFailed(error);
        return {ScpiStatus::WriteFailed, error};
    }

    m_transport->flush();
    emit commandSent(command);
    return {};
}

ScpiReply InstrumentSession::query(const QString &command, int timeoutMs)
{
    if (!isConnected()) {
        return {ScpiStatus::NotConnected, {}, QStringLiteral("not connected")};
    }

    QueryScope scope(m_querying);
    clearPendingInput();

    const ScpiWriteResult writeResult = send(command);
    if (!writeResult.isSuccess()) {
        return {writeResult.status, {}, writeResult.error};
    }

    QByteArray response;
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeoutMs) {
        if (m_transport->bytesAvailable() > 0) {
            response += m_transport->readAll();
            if (response.contains('\n')) {
                break;
            }
            continue;
        }

        const int remainMs = qMax(1, timeoutMs - int(timer.elapsed()));
        if (!m_transport->waitForReadyRead(qMin(100, remainMs))) {
            QCoreApplication::processEvents();
            continue;
        }
        response += m_transport->readAll();
        if (response.contains('\n')) {
            break;
        }
    }

    QString text = QString::fromUtf8(response).trimmed();
    if (text.isEmpty()) {
        return {ScpiStatus::Timeout, {}, QStringLiteral("query timeout")};
    }

    text.replace('\r', '\n');
    const QStringList lines = text.split('\n',
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
                                         Qt::SkipEmptyParts
#else
                                         QString::SkipEmptyParts
#endif
                                         );
    return {ScpiStatus::Success, lines.isEmpty() ? text : lines.first().trimmed(), {}};
}

BinaryBlockReply InstrumentSession::queryBinaryBlock(const QString &command, int timeoutMs)
{
    if (!isConnected()) {
        return {ScpiStatus::NotConnected, {}, 0, QStringLiteral("not connected")};
    }

    QueryScope scope(m_querying);
    clearPendingInput();

    const ScpiWriteResult writeResult = send(command);
    if (!writeResult.isSuccess()) {
        return {writeResult.status, {}, 0, writeResult.error};
    }

    QByteArray response;
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeoutMs) {
        if (m_transport->bytesAvailable() == 0) {
            const int remainMs = qMax(1, timeoutMs - int(timer.elapsed()));
            if (!m_transport->waitForReadyRead(qMin(100, remainMs))) {
                QCoreApplication::processEvents();
                continue;
            }
        }

        response += m_transport->readAll();
        if (response.size() < 2) {
            continue;
        }
        if (response.at(0) != '#') {
            break;
        }

        const int digitsCount = response.at(1) - '0';
        if (digitsCount < 0 || digitsCount > 9) {
            break;
        }

        const int headerLength = 2 + digitsCount;
        if (response.size() < headerLength) {
            continue;
        }

        bool ok = false;
        const int payloadLength = response.mid(2, digitsCount).toInt(&ok);
        if (!ok) {
            break;
        }
        if (response.size() >= headerLength + payloadLength) {
            response = response.left(headerLength + payloadLength);
            break;
        }
    }

    if (response.isEmpty()) {
        return {ScpiStatus::Timeout, {}, 0, QStringLiteral("binary query timeout")};
    }
    if (response.at(0) != '#' || response.size() < 2) {
        return {ScpiStatus::ProtocolError, {}, response.size(),
                QStringLiteral("binary response is not a block")};
    }

    const int digitsCount = response.at(1) - '0';
    const int headerLength = 2 + digitsCount;
    bool ok = false;
    const int payloadLength = response.mid(2, digitsCount).toInt(&ok);
    if (!ok || response.size() < headerLength + payloadLength) {
        return {ScpiStatus::ProtocolError, {}, response.size(),
                QStringLiteral("invalid binary block")};
    }

    return {ScpiStatus::Success,
            response.mid(headerLength, payloadLength),
            response.size(),
            {}};
}

void InstrumentSession::clearPendingInput()
{
    while (m_transport->bytesAvailable() > 0) {
        m_transport->readAll();
    }
}
