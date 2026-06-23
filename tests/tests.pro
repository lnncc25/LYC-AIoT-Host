TEMPLATE = subdirs
CONFIG += ordered

!versionAtLeast(QT_VERSION, 5.12.9) {
    error("AIoT_Test tests require Qt 5.12.9 or newer")
}

SUBDIRS += \
    unit \
    integration \
    case81_integration \
    case82_integration

unit.file = unit/unit.pro
integration.file = integration/integration.pro
case81_integration.file = integration/case81_probe.pro
case82_integration.file = integration/case82_probe.pro
