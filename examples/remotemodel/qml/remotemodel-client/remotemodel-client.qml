import QtQuick 2.4
import QtRemoteModel 0.1

ListView {
    id: window
    width: 300
    height: 200

    model: RemoteModelClient {
    }

    delegate: Row {
        spacing: 10
        Text { text: model.name }
        Text { text: '$%1'.arg(model.cost) }
    }
}
