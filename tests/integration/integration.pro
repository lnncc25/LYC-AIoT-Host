QT += core network
QT -= gui

CONFIG += console c++17
CONFIG -= app_bundle

TARGET = scpi_probe

INCLUDEPATH += \
    ../../src/devices \
    ../../src/devices/scpi

SOURCES += \
    ../../src/devices/scpi/instrumentsession.cpp \
    ../../src/devices/scpi/tcpscpitransport.cpp \
    scpi_probe.cpp

HEADERS += \
    ../../src/devices/scpi/instrumentsession.h \
    ../../src/devices/scpi/iscpitransport.h \
    ../../src/devices/scpi/scpitypes.h \
    ../../src/devices/scpi/tcpscpitransport.h
