#include <QtNetwork/QHostAddress>
#include <QtWidgets>
#include <QtRemoteModel/QRemoteModelClient>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    QTreeView view;
    QRemoteModelClient client;
    client.connectToHost(QHostAddress::LocalHost, 7174);
    view.setModel(&client);
    view.show();

    return app.exec();
}
