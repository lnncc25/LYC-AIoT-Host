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
    ../fakes

SOURCES += \
    ../../src/core/export/csvutils.cpp \
    ../../src/core/paths/outputpaths.cpp \
    ../../src/core/test/testtypes.cpp \
    ../../src/devices/analyzer4071.cpp \
    ../../src/devices/generator1466.cpp \
    ../../src/devices/scpi/instrumentsession.cpp \
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
    ../fakes/fakescpitransport.h
