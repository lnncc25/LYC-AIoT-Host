#include "analyzer4071.h"

#include "scpi/instrumentsession.h"

Analyzer4071::Analyzer4071(InstrumentSession *session)
    : m_session(session)
{
}

InstrumentSession *Analyzer4071::session() const
{
    return m_session;
}

bool Analyzer4071::isConnected() const
{
    return m_session && m_session->isConnected();
}

ScpiReply Analyzer4071::identify(int timeoutMs)
{
    return m_session->query(QStringLiteral("*IDN?"), timeoutMs);
}

ScpiReply Analyzer4071::readError(int timeoutMs)
{
    return m_session->query(QStringLiteral(":SYSTem:ERRor?"), timeoutMs);
}

ScpiReply Analyzer4071::measureBasicVoltage(int timeoutMs)
{
    return m_session->query(QStringLiteral("MEAS:VOLT:DC?"), timeoutMs);
}

bool Analyzer4071::stopMeasurement()
{
    const ScpiWriteResult abortResult = m_session->send(QStringLiteral(":ABORt"));
    const ScpiWriteResult continuousResult =
        m_session->send(QStringLiteral(":INITiate:CONTinuous OFF"));
    m_session->waitForBytesWritten(500);
    return abortResult.isSuccess() && continuousResult.isSuccess();
}
