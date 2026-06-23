QT += core gui widgets network serialport
QT += core gui widgets network serialport charts
QT += charts
CONFIG += c++17
DEFINES += APP_OUTPUT_ROOT=\\\"$$PWD\\\"

!versionAtLeast(QT_VERSION, 5.12.9) {
    error("AIoT_Test requires Qt 5.12.9 or newer")
}

INCLUDEPATH += \
    src/core/export \
    src/ui \
    src/ui/dialogs

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    src/app/main.cpp \
    src/core/export/csvutils.cpp \
    src/ui/mainwindow.cpp \
    src/ui/dialogs/connectiondialog.cpp

HEADERS += \
    src/core/export/csvutils.h \
    src/ui/mainwindow.h \
    src/ui/dialogs/connectiondialog.h

FORMS += \
    src/ui/mainwindow.ui

TRANSLATIONS += \
    i18n/AIoT_Test_zh_CN.ts

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resources/qrc/resources.qrc

DISTFILES += \
    resources/qss/style.qss \
    docs/merge_8_2_8_6_review.md \
    images/README.md
