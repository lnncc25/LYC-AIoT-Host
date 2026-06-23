QT += core network
QT -= gui

CONFIG += console c++17
CONFIG -= app_bundle

TARGET = case85_probe

INCLUDEPATH += \
    ../../src/core/paths \
    ../../src/core/test \
    ../../src/devices \
    ../../src/devices/scpi \
    ../../src/testcases \
    ../../src/testcases/case85

SOURCES += \
    ../../src/core/paths/outputpaths.cpp \
    ../../src/core/test/testtypes.cpp \
    ../../src/devices/analyzer4071.cpp \
    ../../src/devices/generator1466.cpp \
    ../../src/devices/scpi/instrumentsession.cpp \
    ../../src/devices/scpi/tcpscpitransport.cpp \
    ../../src/testcases/case85/case85runcontroller.cpp \
    ../../src/testcases/case85/case85worker.cpp \
    ../../src/testcases/case85/testcase85.cpp \
    case85_probe.cpp

HEADERS += \
    ../../src/core/paths/outputpaths.h \
    ../../src/core/test/testtypes.h \
    ../../src/devices/analyzer4071.h \
    ../../src/devices/generator1466.h \
    ../../src/devices/scpi/instrumentsession.h \
    ../../src/devices/scpi/iscpitransport.h \
    ../../src/devices/scpi/scpitypes.h \
    ../../src/devices/scpi/tcpscpitransport.h \
    ../../src/testcases/case85/case85cancellationtoken.h \
    ../../src/testcases/case85/case85model.h \
    ../../src/testcases/case85/case85runcontroller.h \
    ../../src/testcases/case85/case85worker.h \
    ../../src/testcases/case85/icase85resultpresenter.h \
    ../../src/testcases/case85/testcase85.h \
    ../../src/testcases/itestcase.h \
    ../../src/testcases/itesteventsink.h
