QT += core network
QT -= gui

CONFIG += console c++17
CONFIG -= app_bundle

TARGET = case81_probe

INCLUDEPATH += \
    ../../src/core/test \
    ../../src/devices \
    ../../src/devices/scpi \
    ../../src/testcases \
    ../../src/testcases/case81

SOURCES += \
    ../../src/core/test/testtypes.cpp \
    ../../src/devices/analyzer4071.cpp \
    ../../src/devices/generator1466.cpp \
    ../../src/devices/scpi/instrumentsession.cpp \
    ../../src/devices/scpi/tcpscpitransport.cpp \
    ../../src/testcases/case81/testcase81.cpp \
    case81_probe.cpp

HEADERS += \
    ../../src/core/test/testtypes.h \
    ../../src/devices/analyzer4071.h \
    ../../src/devices/generator1466.h \
    ../../src/devices/scpi/instrumentsession.h \
    ../../src/devices/scpi/iscpitransport.h \
    ../../src/devices/scpi/scpitypes.h \
    ../../src/devices/scpi/tcpscpitransport.h \
    ../../src/testcases/case81/case81model.h \
    ../../src/testcases/case81/icase81resultpresenter.h \
    ../../src/testcases/case81/testcase81.h \
    ../../src/testcases/itestcase.h \
    ../../src/testcases/itestcaseview.h \
    ../../src/testcases/itesteventsink.h
