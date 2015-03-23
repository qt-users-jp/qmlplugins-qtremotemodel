/* Copyright (c) 2015 Tasuku Suzuki.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Tasuku Suzuki nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL TASUKU SUZUKI BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "qremotemodelserver.h"

#include <QtCore/QAbstractItemModel>
#include <QtCore/QUuid>

#include <QtNetwork/QTcpServer>
#include <QtNetwork/QTcpSocket>

class QRemoteModelServer::Private : public QTcpServer
{
    Q_OBJECT
public:
    Private(QRemoteModelServer *parent);
    ~Private();

    void connectModel();
    void disconnectModel();

private:
    QVariant index(const QVariantList &args);
    QVariant parent(const QVariantList &args);
    QVariant columnCount(const QVariantList &args);
    QVariant rowCount(const QVariantList &args);
    QVariant data(const QVariantList &args);
    QVariant canFetchMore(const QVariantList &args);
    QVariant flags(const QVariantList &args);
    QVariant buddy(const QVariantList &args);
    QVariant headerData(const QVariantList &args);
    QVariant hasChildren(const QVariantList &args);
    QVariant submit(const QVariantList &args);
    QVariant fetchMore(const QVariantList &args);
    QVariant sibling(const QVariantList &args);
    QVariant roleNames(const QVariantList &args);

private slots:
    void readData();
    void disconnected();

    void modelDestroyed();
    void dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles);
    void headerDataChanged(Qt::Orientation orientation, int first, int last);

    void rowsAboutToBeInserted(const QModelIndex &parent, int first, int last);
    void rowsInserted(const QModelIndex &parent, int first, int last);
    void rowsAboutToBeMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destinationParent, int destinationRow);
    void rowsMoved(const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row);
    void rowsAboutToBeRemoved(const QModelIndex &parent, int first, int last);
    void rowsRemoved(const QModelIndex &parent, int first, int last);

    void columnsAboutToBeInserted(const QModelIndex &parent, int first, int last);
    void columnsInserted(const QModelIndex &parent, int first, int last);
    void columnsAboutToBeMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destinationParent, int destinationColumn);
    void columnsMoved(const QModelIndex &parent, int start, int end, const QModelIndex &destination, int column);
    void columnsAboutToBeRemoved(const QModelIndex &parent, int first, int last);
    void columnsRemoved(const QModelIndex &parent, int first, int last);
    void modelReset();
    void layoutChanged();

private:
    void methodReturn(QTcpSocket *socket, const QUuid &uuid, const QVariant &ret = QVariant());
    void broadcast(const QByteArray &name, const QVariantList &args = QVariantList());

protected:
    virtual void incomingConnection(qintptr socketDescriptor);

private:
    QRemoteModelServer *q;

public:
    QAbstractItemModel *model;
    QList<QTcpSocket *> clients;
};

QRemoteModelServer::Private::Private(QRemoteModelServer *parent)
    : QTcpServer(parent)
    , q(parent)
    , model(Q_NULLPTR)
{
}

QRemoteModelServer::Private::~Private()
{
}

void QRemoteModelServer::Private::incomingConnection(qintptr socketDescriptor)
{
    QTcpSocket *client = new QTcpSocket(this);
    client->setSocketDescriptor(socketDescriptor);
    connect(client, SIGNAL(readyRead()), this, SLOT(readData()));
    connect(client, SIGNAL(disconnected()), this, SLOT(disconnected()));
    clients.append(client);
}

void QRemoteModelServer::Private::readData()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    while (socket->bytesAvailable() > QtRemoteModel::HeaderLength) {
        QByteArray header = socket->peek(QtRemoteModel::HeaderLength);
        qint64 length = 0;
        length |= header.at(0) << 24;
        length |= header.at(1) << 16;
        length |= header.at(2) <<  8;
        length |= header.at(3);
        if (socket->bytesAvailable() >= QtRemoteModel::HeaderLength + length) {
            socket->read(QtRemoteModel::HeaderLength);
            QDataStream stream(qUncompress(socket->read(length)));
            QUuid uuid;
            stream >> uuid;
            int type;
            stream >> type;
            switch (type) {
            case QtRemoteModel::MethodCall: {
                QByteArray method;
                stream >> method;
                QVariantList args;
                stream >> args;
                qDebug() << length << uuid << type << method << args;
                if (method == QByteArrayLiteral("index")) {
                    methodReturn(socket, uuid, index(args));
                } else if (method == QByteArrayLiteral("parent")) {
                    methodReturn(socket, uuid, parent(args));
                } else if (method == QByteArrayLiteral("columnCount")) {
                    methodReturn(socket, uuid, columnCount(args));
                } else if (method == QByteArrayLiteral("rowCount")) {
                    methodReturn(socket, uuid, rowCount(args));
                } else if (method == QByteArrayLiteral("data")) {
                    methodReturn(socket, uuid, data(args));
                } else if (method == QByteArrayLiteral("canFetchMore")) {
                    methodReturn(socket, uuid, canFetchMore(args));
                } else if (method == QByteArrayLiteral("flags")) {
                    methodReturn(socket, uuid, flags(args));
                } else if (method == QByteArrayLiteral("buddy")) {
                    methodReturn(socket, uuid, buddy(args));
                } else if (method == QByteArrayLiteral("headerData")) {
                    methodReturn(socket, uuid, headerData(args));
                } else if (method == QByteArrayLiteral("hasChildren")) {
                    methodReturn(socket, uuid, hasChildren(args));
                } else if (method == QByteArrayLiteral("submit")) {
                    methodReturn(socket, uuid, submit(args));
                } else if (method == QByteArrayLiteral("fetchMore")) {
                    methodReturn(socket, uuid, fetchMore(args));
                } else if (method == QByteArrayLiteral("sibling")) {
                    methodReturn(socket, uuid, sibling(args));
                } else if (method == QByteArrayLiteral("roleNames")) {
                    methodReturn(socket, uuid, roleNames(args));
                } else {
                    Q_UNREACHABLE();
                }
                break; }
            default:
                qWarning() << type;
                Q_UNREACHABLE();
            }
        }
    }
}

void QRemoteModelServer::Private::disconnected()
{
    QTcpSocket *socket = qobject_cast<QTcpSocket *>(sender());
    clients.removeOne(socket);
    socket->deleteLater();
}

void QRemoteModelServer::Private::connectModel()
{
    if (!model) return;
    connect(model, SIGNAL(destroyed()),
            this, SLOT(modelDestroyed()));
    connect(model, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)),
            this, SLOT(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
    connect(model, SIGNAL(headerDataChanged(Qt::Orientation,int,int)),
            this, SLOT(headerDataChanged(Qt::Orientation,int,int)));

    connect(model, SIGNAL(rowsAboutToBeInserted(QModelIndex,int,int)),
            this, SLOT(rowsAboutToBeInserted(QModelIndex,int,int)));
    connect(model, SIGNAL(rowsInserted(QModelIndex,int,int)),
            this, SLOT(rowsInserted(QModelIndex,int,int)));
    connect(model, SIGNAL(rowsAboutToBeMoved(QModelIndex,int,int,QModelIndex,int)),
            this, SLOT(rowsAboutToBeMoved(QModelIndex,int,int,QModelIndex,int)));
    connect(model, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)),
            this, SLOT(rowsMoved(QModelIndex,int,int,QModelIndex,int)));
    connect(model, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)),
            this, SLOT(rowsAboutToBeRemoved(QModelIndex,int,int)));
    connect(model, SIGNAL(rowsRemoved(QModelIndex,int,int)),
            this, SLOT(rowsRemoved(QModelIndex,int,int)));

    connect(model, SIGNAL(columnsAboutToBeInserted(QModelIndex,int,int)),
            this, SLOT(columnsAboutToBeInserted(QModelIndex,int,int)));
    connect(model, SIGNAL(columnsInserted(QModelIndex,int,int)),
            this, SLOT(columnsInserted(QModelIndex,int,int)));
    connect(model, SIGNAL(columnsAboutToBeMoved(QModelIndex,int,int,QModelIndex,int)),
            this, SLOT(columnsAboutToBeMoved(QModelIndex,int,int,QModelIndex,int)));
    connect(model, SIGNAL(columnsMoved(QModelIndex,int,int,QModelIndex,int)),
            this, SLOT(columnsMoved(QModelIndex,int,int,QModelIndex,int)));
    connect(model, SIGNAL(columnsAboutToBeRemoved(QModelIndex,int,int)),
            this, SLOT(columnsAboutToBeRemoved(QModelIndex,int,int)));
    connect(model, SIGNAL(columnsRemoved(QModelIndex,int,int)),
            this, SLOT(columnsRemoved(QModelIndex,int,int)));

    connect(model, SIGNAL(modelReset()), this, SLOT(modelReset()));
    connect(model, SIGNAL(layoutChanged()), this, SLOT(layoutChanged()));
}

void QRemoteModelServer::Private::disconnectModel()
{
    if (!model) return;
    disconnect(model, SIGNAL(destroyed()),
               this, SLOT(modelDestroyed()));
    disconnect(model, SIGNAL(dataChanged(QModelIndex,QModelIndex,QVector<int>)),
               this, SLOT(dataChanged(QModelIndex,QModelIndex,QVector<int>)));
    disconnect(model, SIGNAL(headerDataChanged(Qt::Orientation,int,int)),
               this, SLOT(headerDataChanged(Qt::Orientation,int,int)));

    disconnect(model, SIGNAL(rowsAboutToBeInserted(QModelIndex,int,int)),
               this, SLOT(rowsAboutToBeInserted(QModelIndex,int,int)));
    disconnect(model, SIGNAL(rowsInserted(QModelIndex,int,int)),
               this, SLOT(rowsInserted(QModelIndex,int,int)));
    disconnect(model, SIGNAL(rowsMoved(QModelIndex,int,int,QModelIndex,int)),
               this, SLOT(rowsMoved(QModelIndex,int,int,QModelIndex,int)));
    disconnect(model, SIGNAL(rowsAboutToBeRemoved(QModelIndex,int,int)),
               this, SLOT(rowsAboutToBeRemoved(QModelIndex,int,int)));
    disconnect(model, SIGNAL(rowsRemoved(QModelIndex,int,int)),
               this, SLOT(rowsRemoved(QModelIndex,int,int)));

    disconnect(model, SIGNAL(columnsAboutToBeInserted(QModelIndex,int,int)),
               this, SLOT(columnsAboutToBeInserted(QModelIndex,int,int)));
    disconnect(model, SIGNAL(columnsInserted(QModelIndex,int,int)),
               this, SLOT(columnsInserted(QModelIndex,int,int)));
    disconnect(model, SIGNAL(columnsMoved(QModelIndex,int,int,QModelIndex,int)),
               this, SLOT(columnsMoved(QModelIndex,int,int,QModelIndex,int)));
    disconnect(model, SIGNAL(columnsAboutToBeRemoved(QModelIndex,int,int)),
               this, SLOT(columnsAboutToBeRemoved(QModelIndex,int,int)));
    disconnect(model, SIGNAL(columnsRemoved(QModelIndex,int,int)),
               this, SLOT(columnsRemoved(QModelIndex,int,int)));

    disconnect(model, SIGNAL(modelReset()), this, SLOT(modelReset()));
    disconnect(model, SIGNAL(layoutChanged()), this, SLOT(layoutChanged()));
}

QVariant QRemoteModelServer::Private::index(const QVariantList &args)
{
    QVariant ret;
    if (model) {
        int i = 0;
        int row = args.at(i++).toInt();
        int column = args.at(i++).toInt();
        QModelIndex parent = QtRemoteModel::toModelIndex(model, args.at(i++));
        QModelIndex index = model->index(row, column, parent);
        ret = QtRemoteModel::fromModelIndex(index);
    }
    return ret;
}

QVariant QRemoteModelServer::Private::parent(const QVariantList &args)
{
    QVariant ret;
    if (model) {
        int i = 0;
        QModelIndex child = QtRemoteModel::toModelIndex(model, args.at(i++));
        QModelIndex parent = model->parent(child);
        ret = QtRemoteModel::fromModelIndex(parent);
    }
    return ret;
}

QVariant QRemoteModelServer::Private::columnCount(const QVariantList &args)
{
    QVariant ret;
    if (model) {
        int i = 0;
        QModelIndex parent;
        if (args.length() > i) {
            parent = QtRemoteModel::toModelIndex(model, args.at(i++));
        }
        ret = model->columnCount(parent);
    }
    return ret;
}

QVariant QRemoteModelServer::Private::rowCount(const QVariantList &args)
{
    QVariant ret;
    if (model) {
        int i = 0;
        QModelIndex parent;
        if (args.length() > i) {
            parent = QtRemoteModel::toModelIndex(model, args.at(i++));
        }
        ret = model->rowCount(parent);
    }
    return ret;
}

QVariant QRemoteModelServer::Private::data(const QVariantList &args)
{
    QVariant ret;
    if (model) {
        int i = 0;
        QModelIndex index = QtRemoteModel::toModelIndex(model, args.at(i++));
        int role = args.at(i++).toInt();
        ret = model->data(index, role);
    }
    return ret;
}

QVariant QRemoteModelServer::Private::canFetchMore(const QVariantList &args)
{
    QVariant ret;
    if (model) {
        int i = 0;
        QModelIndex parent = QtRemoteModel::toModelIndex(model, args.at(i++));
        ret = model->canFetchMore(parent);
    }
    return ret;
}

QVariant QRemoteModelServer::Private::flags(const QVariantList &args)
{
    QVariant ret;
    if (model) {
        int i = 0;
        QModelIndex index = QtRemoteModel::toModelIndex(model, args.at(i++));
        ret = static_cast<int>(model->flags(index));
    }
    return ret;
}

QVariant QRemoteModelServer::Private::buddy(const QVariantList &args)
{
    QVariant ret;
    if (model) {
        int i = 0;
        QModelIndex index = QtRemoteModel::toModelIndex(model, args.at(i++));
        QModelIndex buddy = model->buddy(index);
        ret = QtRemoteModel::fromModelIndex(buddy);
    }
    return ret;
}

QVariant QRemoteModelServer::Private::headerData(const QVariantList &args)
{
    QVariant ret;
    if (model) {
        int i = 0;
        int section = args.at(i++).toInt();
        Qt::Orientation orientation = static_cast<Qt::Orientation>(args.at(i++).toInt());
        int role = args.at(i++).toInt();
        ret = model->headerData(section, orientation, role);
    }
    return ret;
}

QVariant QRemoteModelServer::Private::hasChildren(const QVariantList &args)
{
    QVariant ret;
    if (model) {
        int i = 0;
        QModelIndex parent = QtRemoteModel::toModelIndex(model, args.at(i++));
        ret = model->hasChildren(parent);
    }
    return ret;
}

QVariant QRemoteModelServer::Private::submit(const QVariantList &args)
{
    Q_UNUSED(args)
    QVariant ret;
    if (model) {
        ret = model->submit();
    }
    return ret;
}

QVariant QRemoteModelServer::Private::fetchMore(const QVariantList &args)
{
    QVariant ret;
    if (model) {
        int i = 0;
        QModelIndex parent = QtRemoteModel::toModelIndex(model, args.at(i++));
        model->fetchMore(parent);
    }
    return ret;
}

QVariant QRemoteModelServer::Private::sibling(const QVariantList &args)
{
    QVariant ret;
    if (model) {
        int i = 0;
        int row = args.at(i++).toInt();
        int column = args.at(i++).toInt();
        QModelIndex idx = QtRemoteModel::toModelIndex(model, args.at(i++));
        QModelIndex sibling = model->sibling(row, column, idx);
        ret = QtRemoteModel::fromModelIndex(sibling);
    }
    return ret;
}

QVariant QRemoteModelServer::Private::roleNames(const QVariantList &args)
{
    Q_UNUSED(args)
    QVariant ret;
    if (model) {
        QHash<QString, QVariant> hash;
        QHashIterator<int, QByteArray> i(model->roleNames());
        while(i.hasNext()) {
            i.next();
            hash.insert(QString::number(i.key()), i.value());
        }
        ret = hash;
    }
    return ret;
}
void QRemoteModelServer::Private::modelDestroyed()
{
    model = nullptr;
}

void QRemoteModelServer::Private::dataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    broadcast("dataChanged", QVariantList() << QtRemoteModel::fromModelIndex(topLeft) << QtRemoteModel::fromModelIndex(bottomRight) << QtRemoteModel::toVariant(roles));
}

void QRemoteModelServer::Private::headerDataChanged(Qt::Orientation orientation, int first, int last)
{
    broadcast("headerDataChanged", QVariantList() << orientation << first << last);
}

void QRemoteModelServer::Private::rowsAboutToBeInserted(const QModelIndex &parent, int first, int last)
{
    broadcast("rowsAboutToBeInserted", QVariantList() << QtRemoteModel::fromModelIndex(parent) << first << last);
}

void QRemoteModelServer::Private::rowsInserted(const QModelIndex &parent, int first, int last)
{
    broadcast("rowsInserted", QVariantList() << QtRemoteModel::fromModelIndex(parent) << first << last);
}

void QRemoteModelServer::Private::rowsAboutToBeMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destinationParent, int destinationRow)
{
    broadcast("rowsAboutToBeMoved", QVariantList() << QtRemoteModel::fromModelIndex(sourceParent) << sourceStart << sourceEnd << QtRemoteModel::fromModelIndex(destinationParent) << destinationRow);
}

void QRemoteModelServer::Private::rowsMoved(const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row)
{
    broadcast("rowsMoved", QVariantList() << QtRemoteModel::fromModelIndex(parent) << start << end << QtRemoteModel::fromModelIndex(destination) << row);
}

void QRemoteModelServer::Private::rowsAboutToBeRemoved(const QModelIndex &parent, int first, int last)
{
    broadcast("rowsAboutToBeRemoved", QVariantList() << QtRemoteModel::fromModelIndex(parent) << first << last);
}

void QRemoteModelServer::Private::rowsRemoved(const QModelIndex &parent, int first, int last)
{
    broadcast("rowsRemoved", QVariantList() << QtRemoteModel::fromModelIndex(parent) << first << last);
}

void QRemoteModelServer::Private::columnsAboutToBeInserted(const QModelIndex &parent, int first, int last)
{
    broadcast("columnsAboutToBeInserted", QVariantList() << QtRemoteModel::fromModelIndex(parent) << first << last);
}

void QRemoteModelServer::Private::columnsInserted(const QModelIndex &parent, int first, int last)
{
    broadcast("columnsInserted", QVariantList() << QtRemoteModel::fromModelIndex(parent) << first << last);
}

void QRemoteModelServer::Private::columnsAboutToBeMoved(const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destinationParent, int destinationColumn)
{
    broadcast("columnsAboutToBeMoved", QVariantList() << QtRemoteModel::fromModelIndex(sourceParent) << sourceStart << sourceEnd << QtRemoteModel::fromModelIndex(destinationParent) << destinationColumn);
}

void QRemoteModelServer::Private::columnsMoved(const QModelIndex &parent, int start, int end, const QModelIndex &destination, int column)
{
    broadcast("columnsMoved", QVariantList() << QtRemoteModel::fromModelIndex(parent) << start << end << QtRemoteModel::fromModelIndex(destination) << column);
}

void QRemoteModelServer::Private::columnsAboutToBeRemoved(const QModelIndex &parent, int first, int last)
{
    broadcast("columnsAboutToBeRemoved", QVariantList() << QtRemoteModel::fromModelIndex(parent) << first << last);
}

void QRemoteModelServer::Private::columnsRemoved(const QModelIndex &parent, int first, int last)
{
    broadcast("columnsRemoved", QVariantList() << QtRemoteModel::fromModelIndex(parent) << first << last);
}

void QRemoteModelServer::Private::modelReset()
{
    broadcast("modelReset");
}

void QRemoteModelServer::Private::layoutChanged()
{
    broadcast("layoutChanged");
}

void QRemoteModelServer::Private::methodReturn(QTcpSocket *socket, const QUuid &uuid, const QVariant &ret)
{
    QByteArray response;
    {
        QDataStream stream(&response, QIODevice::WriteOnly);
        stream << uuid;
        stream << QtRemoteModel::MethodReturn;
        stream << ret;
    }
    response = qCompress(response);
    int length = response.length();
    qDebug() << length << uuid << ret;
    QByteArray header(QtRemoteModel::HeaderLength, Qt::Uninitialized);
    for (int i = 0; i < QtRemoteModel::HeaderLength; i++) {
        header[0] = (length & 0xff000000) << 24;
        header[1] = (length & 0x00ff0000) << 16;
        header[2] = (length & 0x0000ff00) <<  8;
        header[3] = (length & 0x000000ff);
    }
    if (socket->write(header) != header.length())
        Q_UNREACHABLE();
    if (socket->write(response) != response.length())
        Q_UNREACHABLE();
}

void QRemoteModelServer::Private::broadcast(const QByteArray &signal, const QVariantList &args)
{
    QUuid uuid = QUuid::createUuid();
    QByteArray data;
    {
        QDataStream stream(&data, QIODevice::WriteOnly);
        stream << uuid;
        stream << QtRemoteModel::EmitSignal;
        stream << signal;
        stream << args;
    }
    data = qCompress(data);
    int length = data.length();
    qDebug() << length << uuid << signal << args;
    QByteArray header(QtRemoteModel::HeaderLength, Qt::Uninitialized);
    for (int i = 0; i < QtRemoteModel::HeaderLength; i++) {
        header[0] = (length & 0xff000000) << 24;
        header[1] = (length & 0x00ff0000) << 16;
        header[2] = (length & 0x0000ff00) <<  8;
        header[3] = (length & 0x000000ff);
    }
    foreach (QTcpSocket *socket, clients) {
        if (socket->write(header) != header.length())
            Q_UNREACHABLE();
        if (socket->write(data) != data.length())
            Q_UNREACHABLE();
    }
}


QRemoteModelServer::QRemoteModelServer(QObject *parent)
    : QObject(parent)
    , d(new Private(this))
{
}

QRemoteModelServer::~QRemoteModelServer()
{
    delete d;
}

QAbstractItemModel *QRemoteModelServer::model() const
{
    return d->model;
}

void QRemoteModelServer::setModel(QAbstractItemModel *model)
{
    if (d->model == model) return;
    d->disconnectModel();
	d->model = model;
    d->connectModel();
	emit modelChanged(model);
}

bool QRemoteModelServer::isListening() const
{
    return d->isListening();
}

bool QRemoteModelServer::listen(const QHostAddress &address, quint16 port)
{
    return d->listen(address, port);
}

QHostAddress QRemoteModelServer::serverAddress() const
{
    return d->serverAddress();
}

quint16 QRemoteModelServer::serverPort() const
{
    return d->serverPort();
}

#include "qremotemodelserver.moc"
