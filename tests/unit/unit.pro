QT += core network testlib
QT -= gui

CONFIG += console c++17 testcase
CONFIG -= app_bundle

!versionAtLeast(QT_VERSION, 5.12.9) {
    error("AIoT_Test unit tests require Qt 5.12.9 or newer")
}

TARGET = tst_coreutils

DEFINES += APP_OUTPUT_ROOT=\\\"$$PWD/../..\\\"

INCLUDEPATH += \
    ../../src/core/export \
    ../../src/core/logging \
    ../../src/core/paths \
    ../../src/core/test \
    ../../src/devices \
    ../../src/devices/scpi \
    ../../src/testcases \
    ../../src/testcases/case81 \
    ../../src/testcases/case82 \
    ../../src/testcases/case85 \
    ../fakes

SOURCES += \
    ../../src/core/export/csvutils.cpp \
    ../../src/core/paths/outputpaths.cpp \
    ../../src/core/test/testtypes.cpp \
    ../../src/devices/analyzer4071.cpp \
    ../../src/devices/generator1466.cpp \
    ../../src/devices/scpi/instrumentsession.cpp \
    ../../src/devices/scpi/tcpscpitransport.cpp \
    ../../src/testcases/case81/case81runcontroller.cpp \
    ../../src/testcases/case81/case81worker.cpp \
    ../../src/testcases/case81/testcase81.cpp \
    ../../src/testcases/case82/case82powerpoints.cpp \
    ../../src/testcases/case82/case82runcontroller.cpp \
    ../../src/testcases/case82/case82worker.cpp \
    ../../src/testcases/case82/testcase82.cpp \
    ../../src/testcases/case85/case85runcontroller.cpp \
    ../../src/testcases/case85/case85worker.cpp \
    ../../src/testcases/case85/testcase85.cpp \
    ../../src/testcases/testcaseregistry.cpp \
    ../fakes/fakescpitransport.cpp \
    tst_coreutils.cpp

HEADERS += \
    ../../src/core/export/csvutils.h \
    ../../src/core/logging/logentry.h \
    ../../src/core/paths/outputpaths.h \
    ../../src/core/test/testtypes.h \
    ../../src/devices/analyzer4071.h \
    ../../src/devices/generator1466.h \
    ../../src/devices/scpi/instrumentsession.h \
    ../../src/devices/scpi/iscpitransport.h \
    ../../src/devices/scpi/scpitypes.h \
    ../../src/devices/scpi/tcpscpitransport.h \
    ../../src/testcases/case81/case81cancellationtoken.h \
    ../../src/testcases/case81/case81model.h \
    ../../src/testcases/case81/case81runconfig.h \
    ../../src/testcases/case81/case81runcontroller.h \
    ../../src/testcases/case81/case81worker.h \
    ../../src/testcases/case81/icase81resultpresenter.h \
    ../../src/testcases/case81/testcase81.h \
    ../../src/testcases/case82/case82cancellationtoken.h \
    ../../src/testcases/case82/case82constants.h \
    ../../src/testcases/case82/case82model.h \
    ../../src/testcases/case82/case82powerpoints.h \
    ../../src/testcases/case82/case82runcontroller.h \
    ../../src/testcases/case82/case82worker.h \
    ../../src/testcases/case82/icase82resultpresenter.h \
    ../../src/testcases/case82/testcase82.h \
    ../../src/testcases/case85/case85cancellationtoken.h \
    ../../src/testcases/case85/case85model.h \
    ../../src/testcases/case85/case85runcontroller.h \
    ../../src/testcases/case85/case85worker.h \
    ../../src/testcases/case85/icase85resultpresenter.h \
    ../../src/testcases/case85/testcase85.h \
    ../../src/testcases/itestcase.h \
    ../../src/testcases/itestcaseview.h \
    ../../src/testcases/itesteventsink.h \
    ../../src/testcases/testcaseregistry.h \
    ../fakes/fakescpitransport.h
