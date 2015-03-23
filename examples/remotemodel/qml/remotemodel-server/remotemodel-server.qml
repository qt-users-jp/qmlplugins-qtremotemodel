import QtQuick 2.4
import QtRemoteModel 0.1

ListView {
    id: window
    width: 300
    height: 200

    Timer {
        id: timer
        interval: 5000
        repeat: true
        running: true
        onTriggered: {
            listModel.insert(
                        Math.floor(Math.random() * (listModel.count + 1)),
                        {'name': Qt.formatDateTime(new Date, 'hh:mm:ss.zzz'), 'cost': Math.floor(Math.random() * 100)}
                        )
        }
    }

    model: ListModel {
        id: listModel
    }

    delegate: Row {
        spacing: 10
        MouseArea {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: height
            Text {
                anchors.centerIn: parent
                font.pixelSize: parent.width
                text: '\u00d7'
            }
            onClicked: listModel.remove(model.index)
        }
        MouseArea {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: height
            Text {
                anchors.centerIn: parent
                font.pixelSize: parent.width
                text: '\u2191'
            }
            onClicked: if (model.index > 0) listModel.move(model.index, model.index - 1, 1)
        }
        MouseArea {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: height
            Text {
                anchors.centerIn: parent
                font.pixelSize: parent.width
                text: '\u2193'
            }
            onClicked: if (model.index < listModel.count - 1) listModel.move(model.index, model.index + 1, 1)
        }

        Text { text: model.name }
        Text { text: '$%1'.arg(model.cost) }
    }

    RemoteModelServer {
        model: listModel
    }
}
