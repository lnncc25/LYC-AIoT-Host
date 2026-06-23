#include "instrumentsession.h"
#include "tcpscpitransport.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTextStream>
#include <QTimer>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    const QStringList arguments = app.arguments();
    if (arguments.size() != 3) {
        QTextStream(stderr) << "Usage: scpi_probe <host> <port>\n";
        return 2;
    }

    bool portOk = false;
    const uint portValue = arguments.at(2).toUInt(&portOk);
    if (!portOk || portValue > 65535) {
        QTextStream(stderr) << "Invalid port\n";
        return 2;
    }

    InstrumentSession session(new TcpScpiTransport);
    QEventLoop connectLoop;
    QTimer connectTimeout;
    connectTimeout.setSingleShot(true);
    QObject::connect(&session, &InstrumentSession::connected,
                     &connectLoop, &QEventLoop::quit);
    QObject::connect(&connectTimeout, &QTimer::timeout,
                     &connectLoop, &QEventLoop::quit);

    session.connectToHost(arguments.at(1), static_cast<quint16>(portValue));
    connectTimeout.start(3000);
    connectLoop.exec();
    if (!session.isConnected()) {
        QTextStream(stderr) << "Connection failed: " << session.errorString() << '\n';
        return 3;
    }

    const ScpiReply idn = session.query(QStringLiteral("*IDN?"), 2000);
    const ScpiReply error = session.query(QStringLiteral(":SYSTem:ERRor?"), 2000);
    session.disconnectFromHost();

    if (!idn.isSuccess() || !error.isSuccess()) {
        QTextStream(stderr) << "Query failed\n";
        return 4;
    }

    QTextStream(stdout) << "IDN=" << idn.text << '\n'
                        << "ERROR=" << error.text << '\n';
    return 0;
}
