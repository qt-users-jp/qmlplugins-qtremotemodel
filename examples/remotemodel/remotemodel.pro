TEMPLATE = subdirs
SUBDIRS = cpp

!isEmpty(QT.qml.name) {
    SUBDIRS += qml
}

