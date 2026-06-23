QT += core testlib
QT -= gui

CONFIG += console c++17 testcase
CONFIG -= app_bundle

!versionAtLeast(QT_VERSION, 5.12.9) {
    error("AIoT_Test unit tests require Qt 5.12.9 or newer")
}

TARGET = tst_csvutils

INCLUDEPATH += ../../src/core/export

SOURCES += \
    ../../src/core/export/csvutils.cpp \
    tst_csvutils.cpp

HEADERS += \
    ../../src/core/export/csvutils.h
