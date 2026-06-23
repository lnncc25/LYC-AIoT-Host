TEMPLATE = subdirs
CONFIG += ordered

!versionAtLeast(QT_VERSION, 5.12.9) {
    error("AIoT_Test tests require Qt 5.12.9 or newer")
}

SUBDIRS += \
    unit \
    integration

unit.file = unit/unit.pro
integration.file = integration/integration.pro
