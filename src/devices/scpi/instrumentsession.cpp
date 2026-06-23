#include "instrumentsession.h"

#include "iscpitransport.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>

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

ScpiWriteResult cancelledWriteResult(const QString &error)
{
    return {ScpiStatus::Cancelled, error};
}

ScpiReply cancelledReply(const QString &error)
{
    return {ScpiStatus::Cancelled, QString(), error};
}

BinaryBlockReply cancelledBinaryReply(const QString &error)
{
    return {ScpiStatus::Cancelled, QByteArray(), 0, error};
}

}

InstrumentSession::InstrumentSession(IScpiTransport *transport, QObject *parent)
    : QObject(parent)
    , m_transport(transport)
    , m_querying(false)
    , m_defaultCancellation(nullptr)
    , m_queueMutex(new QMutex)
    , m_queueChanged(new QWaitCondition)
    , m_normalQueue(new QList<PendingRequest *>)
    , m_safetyQueue(new QList<PendingRequest *>)
    , m_activeRequest(nullptr)
    , m_nextRequestId(1)
    , m_abortRequested(false)
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

InstrumentSession::~InstrumentSession()
{
    {
        QMutexLocker locker(m_queueMutex);
        m_abortRequested.store(true, std::memory_order_release);
        m_queueChanged->wakeAll();
    }

    delete m_normalQueue;
    delete m_safetyQueue;
    delete m_queueChanged;
    delete m_queueMutex;
}

void InstrumentSession::connectToHost(const QString &host, quint16 port)
{
    {
        QMutexLocker locker(m_queueMutex);
        m_abortRequested.store(false, std::memory_order_release);
    }
    m_transport->connectToHost(host, port);
}

void InstrumentSession::disconnectFromHost()
{
    {
        QMutexLocker locker(m_queueMutex);
        m_abortRequested.store(true, std::memory_order_release);
        m_queueChanged->wakeAll();
    }
    m_transport->disconnectFromHost();
}

bool InstrumentSession::isConnected() const
{
    return m_transport->isConnected();
}

bool InstrumentSession::waitForConnected(int timeoutMs)
{
    return m_transport->waitForConnected(timeoutMs);
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

void InstrumentSession::setDefaultCancellation(const IScpiCancellation *cancellation)
{
    QMutexLocker locker(m_queueMutex);
    m_defaultCancellation = cancellation;
    m_queueChanged->wakeAll();
}

const IScpiCancellation *InstrumentSession::defaultCancellation() const
{
    QMutexLocker locker(m_queueMutex);
    return m_defaultCancellation;
}

bool InstrumentSession::abortActiveRequest()
{
    QMutexLocker locker(m_queueMutex);
    if (!m_activeRequest) {
        return false;
    }
    m_abortRequested.store(true, std::memory_order_release);
    m_queueChanged->wakeAll();
    return true;
}

ScpiWriteResult InstrumentSession::send(const QString &command)
{
    return send(command, ScpiRequestOptions());
}

ScpiWriteResult InstrumentSession::send(const QString &command,
                                        const ScpiRequestOptions &options)
{
    PendingRequest request;
    request.kind = RequestKind::Send;
    request.command = command;
    request.options = options;

    QMutexLocker locker(m_queueMutex);
    request.id = m_nextRequestId++;
    (options.priority == ScpiRequestPriority::Safety
         ? m_safetyQueue
         : m_normalQueue)->append(&request);
    if (options.abortActiveRequest) {
        m_abortRequested.store(true, std::memory_order_release);
    }
    m_queueChanged->wakeAll();

    while (!request.completed) {
        if (requestShouldStop(request.options)) {
            removePendingRequestLocked(&request);
            request.writeResult = cancelledWriteResult(requestStopReason(request.options));
            completePendingRequest(request);
            break;
        }

        if (!m_activeRequest && nextPendingRequestLocked() == &request) {
            removePendingRequestLocked(&request);
            m_activeRequest = &request;
            m_abortRequested.store(false, std::memory_order_release);
            locker.unlock();
            request.writeResult = performSend(request.command, request.options);
            locker.relock();
            finishActiveRequestLocked(&request);
            completePendingRequest(request);
            break;
        }

        m_queueChanged->wait(m_queueMutex, 50);
    }

    return request.writeResult;
}

ScpiReply InstrumentSession::query(const QString &command, int timeoutMs)
{
    return query(command, timeoutMs, ScpiRequestOptions());
}

ScpiReply InstrumentSession::query(const QString &command,
                                   int timeoutMs,
                                   const ScpiRequestOptions &options)
{
    PendingRequest request;
    request.kind = RequestKind::Query;
    request.command = command;
    request.timeoutMs = timeoutMs;
    request.options = options;

    QMutexLocker locker(m_queueMutex);
    request.id = m_nextRequestId++;
    (options.priority == ScpiRequestPriority::Safety
         ? m_safetyQueue
         : m_normalQueue)->append(&request);
    if (options.abortActiveRequest) {
        m_abortRequested.store(true, std::memory_order_release);
    }
    m_queueChanged->wakeAll();

    while (!request.completed) {
        if (requestShouldStop(request.options)) {
            removePendingRequestLocked(&request);
            request.reply = cancelledReply(requestStopReason(request.options));
            completePendingRequest(request);
            break;
        }

        if (!m_activeRequest && nextPendingRequestLocked() == &request) {
            removePendingRequestLocked(&request);
            m_activeRequest = &request;
            m_abortRequested.store(false, std::memory_order_release);
            locker.unlock();
            request.reply = performQuery(request.command, request.timeoutMs, request.options);
            locker.relock();
            finishActiveRequestLocked(&request);
            completePendingRequest(request);
            break;
        }

        m_queueChanged->wait(m_queueMutex, 50);
    }

    return request.reply;
}

BinaryBlockReply InstrumentSession::queryBinaryBlock(const QString &command, int timeoutMs)
{
    return queryBinaryBlock(command, timeoutMs, ScpiRequestOptions());
}

BinaryBlockReply InstrumentSession::queryBinaryBlock(const QString &command,
                                                     int timeoutMs,
                                                     const ScpiRequestOptions &options)
{
    PendingRequest request;
    request.kind = RequestKind::BinaryQuery;
    request.command = command;
    request.timeoutMs = timeoutMs;
    request.options = options;

    QMutexLocker locker(m_queueMutex);
    request.id = m_nextRequestId++;
    (options.priority == ScpiRequestPriority::Safety
         ? m_safetyQueue
         : m_normalQueue)->append(&request);
    if (options.abortActiveRequest) {
        m_abortRequested.store(true, std::memory_order_release);
    }
    m_queueChanged->wakeAll();

    while (!request.completed) {
        if (requestShouldStop(request.options)) {
            removePendingRequestLocked(&request);
            request.binaryReply = cancelledBinaryReply(requestStopReason(request.options));
            completePendingRequest(request);
            break;
        }

        if (!m_activeRequest && nextPendingRequestLocked() == &request) {
            removePendingRequestLocked(&request);
            m_activeRequest = &request;
            m_abortRequested.store(false, std::memory_order_release);
            locker.unlock();
            request.binaryReply =
                performBinaryQuery(request.command, request.timeoutMs, request.options);
            locker.relock();
            finishActiveRequestLocked(&request);
            completePendingRequest(request);
            break;
        }

        m_queueChanged->wait(m_queueMutex, 50);
    }

    return request.binaryReply;
}

ScpiWriteResult InstrumentSession::performSend(const QString &command,
                                               const ScpiRequestOptions &options)
{
    if (requestShouldStop(options)) {
        return cancelledWriteResult(requestStopReason(options));
    }
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

ScpiReply InstrumentSession::performQuery(const QString &command,
                                          int timeoutMs,
                                          const ScpiRequestOptions &options)
{
    if (requestShouldStop(options)) {
        return cancelledReply(requestStopReason(options));
    }
    if (!isConnected()) {
        return {ScpiStatus::NotConnected, QString(), QStringLiteral("not connected")};
    }

    QueryScope scope(m_querying);
    clearPendingInput();

    const ScpiWriteResult writeResult = performSend(command, options);
    if (!writeResult.isSuccess()) {
        return {writeResult.status, QString(), writeResult.error};
    }

    QByteArray response;
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeoutMs) {
        if (requestShouldStop(options)) {
            return cancelledReply(requestStopReason(options));
        }

        if (m_transport->bytesAvailable() > 0) {
            response += m_transport->readAll();
            if (response.contains('\n')) {
                break;
            }
            continue;
        }

        const int remainMs = qMax(1, timeoutMs - int(timer.elapsed()));
        if (!m_transport->waitForReadyRead(qMin(50, remainMs))) {
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
        return {ScpiStatus::Timeout, QString(), QStringLiteral("query timeout")};
    }

    text.replace('\r', '\n');
    const QStringList lines = text.split('\n',
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
                                         Qt::SkipEmptyParts
#else
                                         QString::SkipEmptyParts
#endif
                                         );
    return {ScpiStatus::Success, lines.isEmpty() ? text : lines.first().trimmed(), QString()};
}

BinaryBlockReply InstrumentSession::performBinaryQuery(const QString &command,
                                                       int timeoutMs,
                                                       const ScpiRequestOptions &options)
{
    if (requestShouldStop(options)) {
        return cancelledBinaryReply(requestStopReason(options));
    }
    if (!isConnected()) {
        return {ScpiStatus::NotConnected, QByteArray(), 0, QStringLiteral("not connected")};
    }

    QueryScope scope(m_querying);
    clearPendingInput();

    const ScpiWriteResult writeResult = performSend(command, options);
    if (!writeResult.isSuccess()) {
        return {writeResult.status, QByteArray(), 0, writeResult.error};
    }

    QByteArray response;
    QElapsedTimer timer;
    timer.start();

    while (timer.elapsed() < timeoutMs) {
        if (requestShouldStop(options)) {
            return cancelledBinaryReply(requestStopReason(options));
        }

        if (m_transport->bytesAvailable() == 0) {
            const int remainMs = qMax(1, timeoutMs - int(timer.elapsed()));
            if (!m_transport->waitForReadyRead(qMin(50, remainMs))) {
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
        return {ScpiStatus::Timeout, QByteArray(), 0, QStringLiteral("binary query timeout")};
    }
    if (response.at(0) != '#' || response.size() < 2) {
        return {ScpiStatus::ProtocolError, QByteArray(), response.size(),
                QStringLiteral("binary response is not a block")};
    }

    const int digitsCount = response.at(1) - '0';
    const int headerLength = 2 + digitsCount;
    bool ok = false;
    const int payloadLength = response.mid(2, digitsCount).toInt(&ok);
    if (!ok || response.size() < headerLength + payloadLength) {
        return {ScpiStatus::ProtocolError, QByteArray(), response.size(),
                QStringLiteral("invalid binary block")};
    }

    return {ScpiStatus::Success,
            response.mid(headerLength, payloadLength),
            response.size(),
            QString()};
}

void InstrumentSession::completePendingRequest(PendingRequest &request)
{
    request.completed = true;
    m_queueChanged->wakeAll();
}

void InstrumentSession::finishActiveRequestLocked(PendingRequest *request)
{
    if (m_activeRequest == request) {
        m_activeRequest = nullptr;
    }
    m_abortRequested.store(false, std::memory_order_release);
    m_queueChanged->wakeAll();
}

InstrumentSession::PendingRequest *InstrumentSession::nextPendingRequestLocked() const
{
    if (!m_safetyQueue->isEmpty()) {
        return m_safetyQueue->first();
    }
    if (!m_normalQueue->isEmpty()) {
        return m_normalQueue->first();
    }
    return nullptr;
}

void InstrumentSession::removePendingRequestLocked(PendingRequest *request)
{
    m_normalQueue->removeOne(request);
    m_safetyQueue->removeOne(request);
}

const IScpiCancellation *InstrumentSession::effectiveCancellation(
    const ScpiRequestOptions &options) const
{
    return options.cancellation ? options.cancellation : m_defaultCancellation;
}

bool InstrumentSession::requestShouldStop(const ScpiRequestOptions &options) const
{
    if (options.priority == ScpiRequestPriority::Safety) {
        return false;
    }
    if (m_abortRequested.load(std::memory_order_acquire)) {
        return true;
    }
    const IScpiCancellation *cancellation = effectiveCancellation(options);
    return cancellation && cancellation->isCancellationRequested();
}

QString InstrumentSession::requestStopReason(const ScpiRequestOptions &options) const
{
    if (options.priority != ScpiRequestPriority::Safety
        && m_abortRequested.load(std::memory_order_acquire)) {
        return QStringLiteral("request aborted");
    }
    return QStringLiteral("stop requested");
}

void InstrumentSession::clearPendingInput()
{
    while (m_transport->bytesAvailable() > 0) {
        m_transport->readAll();
    }
}
