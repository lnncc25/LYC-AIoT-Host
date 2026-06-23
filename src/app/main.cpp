#include "mainwindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setStyleSheet("* { font-size: 16pt !important; }");
    a.setWindowIcon(QIcon(":/images/app/logo.png"));
    MainWindow w;
    w.show();
    return a.exec();
}
