TARGET = QtRemoteModel
MODULE = remotemodel
QT = core network

load(qt_module)

HEADERS = qtremotemodel_global.h \
    qremotemodelserver.h \
    qremotemodelclient.h

SOURCES = qtremotemodel_global.cpp \
    qremotemodelserver.cpp \
    qremotemodelclient.cpp

DEFINES += QTREMOTEMODEL_LIBRARY

CONFIG -= create_cmake
