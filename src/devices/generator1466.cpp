#include "generator1466.h"

#include "scpi/instrumentsession.h"

Generator1466::Generator1466(InstrumentSession *session)
    : m_session(session)
{
}

InstrumentSession *Generator1466::session() const
{
    return m_session;
}

bool Generator1466::isConnected() const
{
    return m_session && m_session->isConnected();
}

ScpiReply Generator1466::identify(int timeoutMs)
{
    return m_session->query(QStringLiteral("*IDN?"), timeoutMs);
}

ScpiReply Generator1466::readError(int timeoutMs)
{
    return m_session->query(QStringLiteral(":SYSTem:ERRor?"), timeoutMs);
}
