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
    src/core/logging \
    src/core/paths \
    src/core/test \
    src/devices \
    src/devices/scpi \
    src/testcases \
    src/testcases/case81 \
    src/ui \
    src/ui/dialogs \
    src/ui/presenters

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
    src/core/paths/outputpaths.cpp \
    src/core/test/testtypes.cpp \
    src/devices/analyzer4071.cpp \
    src/devices/generator1466.cpp \
    src/devices/scpi/instrumentsession.cpp \
    src/devices/scpi/tcpscpitransport.cpp \
    src/testcases/case81/testcase81.cpp \
    src/testcases/testcaseregistry.cpp \
    src/ui/mainwindow.cpp \
    src/ui/dialogs/connectiondialog.cpp \
    src/ui/presenters/case81uiadapter.cpp

HEADERS += \
    src/core/export/csvutils.h \
    src/core/logging/logentry.h \
    src/core/paths/outputpaths.h \
    src/core/test/testtypes.h \
    src/devices/analyzer4071.h \
    src/devices/generator1466.h \
    src/devices/scpi/instrumentsession.h \
    src/devices/scpi/iscpitransport.h \
    src/devices/scpi/scpitypes.h \
    src/devices/scpi/tcpscpitransport.h \
    src/testcases/case81/case81model.h \
    src/testcases/case81/icase81resultpresenter.h \
    src/testcases/case81/testcase81.h \
    src/testcases/itestcase.h \
    src/testcases/itestcaseview.h \
    src/testcases/itesteventsink.h \
    src/testcases/testcaseregistry.h \
    src/ui/mainwindow.h \
    src/ui/dialogs/connectiondialog.h \
    src/ui/presenters/case81uiadapter.h

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
