QT += core network
QT -= gui

CONFIG += console c++17
CONFIG -= app_bundle

TARGET = case82_probe

INCLUDEPATH += \
    ../../src/core/test \
    ../../src/devices \
    ../../src/devices/scpi \
    ../../src/testcases \
    ../../src/testcases/case82

SOURCES += \
    ../../src/core/test/testtypes.cpp \
    ../../src/devices/analyzer4071.cpp \
    ../../src/devices/generator1466.cpp \
    ../../src/devices/scpi/instrumentsession.cpp \
    ../../src/devices/scpi/tcpscpitransport.cpp \
    ../../src/testcases/case82/case82powerpoints.cpp \
    ../../src/testcases/case82/case82runcontroller.cpp \
    ../../src/testcases/case82/case82worker.cpp \
    ../../src/testcases/case82/testcase82.cpp \
    case82_probe.cpp

HEADERS += \
    ../../src/core/test/testtypes.h \
    ../../src/devices/analyzer4071.h \
    ../../src/devices/generator1466.h \
    ../../src/devices/scpi/instrumentsession.h \
    ../../src/devices/scpi/iscpitransport.h \
    ../../src/devices/scpi/scpitypes.h \
    ../../src/devices/scpi/tcpscpitransport.h \
    ../../src/testcases/case82/case82cancellationtoken.h \
    ../../src/testcases/case82/case82constants.h \
    ../../src/testcases/case82/case82model.h \
    ../../src/testcases/case82/case82powerpoints.h \
    ../../src/testcases/case82/case82runcontroller.h \
    ../../src/testcases/case82/case82worker.h \
    ../../src/testcases/case82/icase82resultpresenter.h \
    ../../src/testcases/case82/testcase82.h \
    ../../src/testcases/itestcase.h \
    ../../src/testcases/itesteventsink.h
