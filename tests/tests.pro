TEMPLATE = subdirs
CONFIG += ordered

!versionAtLeast(QT_VERSION, 5.12.9) {
    error("AIoT_Test tests require Qt 5.12.9 or newer")
}

SUBDIRS += unit

unit.file = unit/unit.pro
