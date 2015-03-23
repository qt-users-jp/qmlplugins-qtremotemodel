#include <QtGui/QStandardItemModel>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFileSystemModel>
#include <QtWidgets/QTreeView>
#include <QtRemoteModel/QRemoteModelServer>

#include <mcheck.h>

int main(int argc, char **argv)
{
    mtrace();
    QApplication app(argc, argv);

    QRemoteModelServer server;
#if 0
    QFileSystemModel model;
    model.setRootPath(app.applicationDirPath());
#else
    QStandardItemModel model(4, 4);
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            QStandardItem *item = new QStandardItem(QString("row %0, column %1").arg(row).arg(column));
            model.setItem(row, column, item);
        }
    }
#endif

    server.setModel(&model);
    server.listen(QHostAddress::LocalHost, 7174);

    QTreeView view;
    view.setModel(&model);
    view.show();
    int ret = app.exec();
    muntrace();
    return ret;
}
