TARGETPATH = QtRemoteModel

QT = qml remotemodel
LIBS += -L$$QT.remotemodel.libs

SOURCES += main.cpp

load(qml_plugin)

OTHER_FILES = plugins.qmltypes qmldir
