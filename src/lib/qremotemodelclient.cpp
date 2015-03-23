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

#include "qremotemodelclient.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QSize>
#include <QtCore/QEventLoop>
#include <QtCore/QUuid>

#include <QtNetwork/QTcpSocket>

class Node
{
public:
    Node() : row(-1), column(-1), parent(Q_NULLPTR) {}
    Node(int row, int column, Node *parent = Q_NULLPTR)
        : row(row), column(column), parent(parent) {
        if (parent)
            parent->children.append(this);
    }
    ~Node() {
        if (parent)
            parent->children.removeOne(this);
        qDeleteAll(children);
    }

    void check(const char *func, int line) const {
        if (children.isEmpty()) return;

        std::sort(children.begin(), children.end(), [](const Node *a, const Node *b) {
            if (a->row == b->row)
                return a->column < b->column;
            return a->row < b->row;
        });

        bool valid = true;
        if (children.count() != (children.last()->row + 1) * (children.last()->column + 1))
            valid = false;

        foreach (const Node *node, children) {
            qDebug() << node->row << node->column << node;
        }
        if (valid) {
            foreach (const Node *node, children) {
                node->check(func, line);
            }
        } else {
            qDebug() << func << line;
            Q_UNREACHABLE();
        }
    }

    int row;
    int column;
    Node *parent;
    mutable QList<Node *> children;
};

QDebug operator<<(QDebug dbg, const Node *node) {
    dbg.nospace() << "Node {";
    if (node) {
        dbg << " row: " << node->row << "; column: " << node->column << ";";
        if (node->parent)
            dbg << " parent: " << node->parent << ";";
    }
    dbg << "}";
    return dbg.space();
}

class QRemoteModelClient::Private : public QTcpSocket
{
    Q_OBJECT
public:
    Private(QRemoteModelClient *parent);
    ~Private();

    QVariant methodCall(const QByteArray &method, const QVariantList &args = QVariantList());

private slots:
    void init();
    void construct(const QModelIndex &parent = QModelIndex());
    void readData();

    void dataChanged(const QVariantList &args);
    void headerDataChanged(const QVariantList &args);
    void layoutChanged(const QVariantList &args);
    void layoutAboutToBeChanged(const QVariantList &args);

    void rowsAboutToBeInserted(const QVariantList &args);
    void rowsInserted(const QVariantList &args);
    void rowsAboutToBeMoved(const QVariantList &args);
    void rowsMoved(const QVariantList &args);
    void rowsAboutToBeRemoved(const QVariantList &args);
    void rowsRemoved(const QVariantList &args);

    void columnsAboutToBeInserted(const QVariantList &args);
    void columnsInserted(const QVariantList &args);
    void columnsAboutToBeMoved(const QVariantList &args);
    void columnsMoved(const QVariantList &args);
    void columnsAboutToBeRemoved(const QVariantList &args);
    void columnsRemoved(const QVariantList &args);

    void modelAboutToBeReset(const QVariantList &args);
    void modelReset(const QVariantList &args);

private:
    QRemoteModelClient *q;
    QHash<QUuid, QEventLoop *> loops;
    QHash<QUuid, QVariant> returnValues;

public:
    Node *rootNode;
};

QRemoteModelClient::Private::Private(QRemoteModelClient *parent)
    : QTcpSocket(parent)
    , q(parent)
    , rootNode(new Node)
{
    connect(this, SIGNAL(connected()), this, SLOT(init()));
    connect(this, SIGNAL(readyRead()), this, SLOT(readData()));
}

QRemoteModelClient::Private::~Private()
{
    delete rootNode;
}

void QRemoteModelClient::Private::init()
{
    construct();
}

void QRemoteModelClient::Private::construct(const QModelIndex &parent)
{
    Node *parentNode = parent.internalPointer() ? static_cast<Node *>(parent.internalPointer()) : rootNode;
    qDebug() << parentNode->row << parentNode->column << parentNode->parent;
    bool hasChildren = methodCall("hasChildren", QVariantList() << QtRemoteModel::fromModelIndex(parent)).toBool();
    if (hasChildren) {
        int rowCount = methodCall("rowCount", QVariantList() << QtRemoteModel::fromModelIndex(parent)).toInt();
        int columnCount = methodCall("columnCount", QVariantList() << QtRemoteModel::fromModelIndex(parent)).toInt();
        columnsAboutToBeInserted(QVariantList() << parent << 0 << columnCount - 1);
        columnsInserted(QVariantList());
        rowsAboutToBeInserted(QVariantList() << parent << 0 << rowCount - 1);
        rowsInserted(QVariantList() << parent << 0 << rowCount - 1);
        for (int row = 0; row < rowCount; row++) {
            for (int column = 0; column < columnCount; column++) {
                construct(q->index(row, column, parent));
            }
        }
    }
}

QVariant QRemoteModelClient::Private::methodCall(const QByteArray &method, const QVariantList &args)
{
    while (!loops.isEmpty()) {
        QCoreApplication::processEvents();
        qDebug() << method << args << bytesAvailable() << loops;
        readData();
    }
    QUuid uuid = QUuid::createUuid();
    QByteArray request;
    {
        QDataStream in(&request, QIODevice::WriteOnly);
        in << uuid;
        in << QtRemoteModel::MethodCall;
        in << method;
        in << args;
    }
    request = qCompress(request);
    int length = request.length();
    QByteArray header(QtRemoteModel::HeaderLength, Qt::Uninitialized);
    for (int i = 0; i < QtRemoteModel::HeaderLength; i++) {
        header[0] = (length & 0xff000000) >> 24;
        header[1] = (length & 0x00ff0000) >> 16;
        header[2] = (length & 0x0000ff00) >>  8;
        header[3] = (length & 0x000000ff);
    }
    qDebug() << length << method << method << args << uuid << loops.keys() << loops.values();
    if (write(header) != header.length())
        Q_UNREACHABLE();
    if (write(request) != request.length())
        Q_UNREACHABLE();
    QEventLoop loop;
    loops.insert(uuid, &loop);
//    qDebug() << &loop;
    loop.exec();
//    loops.remove(uuid);
    QVariant ret = returnValues.take(uuid);
    qDebug() << ret;
    return ret;
}

void QRemoteModelClient::Private::readData()
{
//    qDebug() << bytesAvailable();
    while (bytesAvailable() >= QtRemoteModel::HeaderLength) {
        QByteArray header = peek(QtRemoteModel::HeaderLength);
        qint64 length = 0;
        length |= header.at(0) << 24;
        length |= header.at(1) << 16;
        length |= header.at(2) <<  8;
        length |= header.at(3);
        if (bytesAvailable() >= QtRemoteModel::HeaderLength + length) {
            if (read(QtRemoteModel::HeaderLength).length() != QtRemoteModel::HeaderLength)
                Q_UNREACHABLE();
            QByteArray data = read(length);
            if (data.length() != length) {
                Q_UNREACHABLE();
            }
            data = qUncompress(data);
            QDataStream stream(data);
            QUuid uuid;
            int type;
            stream >> uuid;
            stream >> type;
//            qDebug() << length << uuid << type;

            switch (type) {
            case QtRemoteModel::MethodReturn:
                if (loops.contains(uuid)) {
                    QVariant returnValue;
                    stream >> returnValue;
//                    qDebug() << returnValue;
                    returnValues.insert(uuid, returnValue);
                    loops.take(uuid)->quit();
                } else {
                    qDebug() << loops << uuid;
                    Q_UNREACHABLE();
                }
                break;
            case QtRemoteModel::EmitSignal: {
                QByteArray signal;
                QVariantList args;
                stream >> signal;
                stream >> args;
                qDebug() << signal << args;
                QMetaObject::invokeMethod(this, signal.constData(), Qt::QueuedConnection, Q_ARG(QVariantList, args));
#if 0
                if (signal == QByteArrayLiteral("dataChanged")) {
                    dataChanged(args);
                } else if (signal == QByteArrayLiteral("headerDataChanged")) {
                    headerDataChanged(args);
                } else if (signal == QByteArrayLiteral("layoutChanged")) {
                    layoutChanged(args);
                } else if (signal == QByteArrayLiteral("layoutAboutToBeChanged")) {
                    layoutAboutToBeChanged(args);
                } else if (signal == QByteArrayLiteral("rowsAboutToBeInserted")) {
                    rowsAboutToBeInserted(args);
                } else if (signal == QByteArrayLiteral("rowsInserted")) {
                    rowsInserted(args);
                } else if (signal == QByteArrayLiteral("rowsAboutToBeMoved")) {
                    rowsAboutToBeMoved(args);
                } else if (signal == QByteArrayLiteral("rowsMoved")) {
                    rowsMoved(args);
                } else if (signal == QByteArrayLiteral("rowsAboutToBeRemoved")) {
                    rowsAboutToBeRemoved(args);
                } else if (signal == QByteArrayLiteral("rowsRemoved")) {
                    rowsRemoved(args);
                } else if (signal == QByteArrayLiteral("columnsAboutToBeInserted")) {
                    columnsAboutToBeInserted(args);
                } else if (signal == QByteArrayLiteral("columnsInserted")) {
                    columnsInserted(args);
                } else if (signal == QByteArrayLiteral("columnsAboutToBeMoved")) {
                    columnsAboutToBeMoved(args);
                } else if (signal == QByteArrayLiteral("columnsMoved")) {
                    columnsMoved(args);
                } else if (signal == QByteArrayLiteral("columnsAboutToBeRemoved")) {
                    columnsAboutToBeRemoved(args);
                } else if (signal == QByteArrayLiteral("columnsRemoved")) {
                    columnsRemoved(args);
                } else if (signal == QByteArrayLiteral("modelAboutToBeReset")) {
                    modelAboutToBeReset(args);
                } else if (signal == QByteArrayLiteral("modelReset")) {
                    modelReset(args);
                } else {
                    qWarning() << signal << args;
                    Q_UNREACHABLE();
                }
#endif
                break; }
            default:
                qWarning() << type;
                Q_UNREACHABLE();
                break;
            }
        }
    }
}

void QRemoteModelClient::Private::dataChanged(const QVariantList &args)
{
    int i = 0;
    QModelIndex topLeft = QtRemoteModel::toModelIndex(q, args.at(i++));
    QModelIndex bottomRight = QtRemoteModel::toModelIndex(q, args.at(i++));
    QVector<int> roles;
    foreach (const QVariant &v, args.at(i++).toList()) {
        roles.append(v.toInt());
    }
    qDebug() << topLeft << bottomRight << roles;
    emit q->dataChanged(topLeft, bottomRight, roles);
    qDebug() << topLeft << bottomRight << roles;
}

void QRemoteModelClient::Private::headerDataChanged(const QVariantList &args)
{
    int i = 0;
    Qt::Orientation orientation = static_cast<Qt::Orientation>(args.at(i++).toInt());
    int first = args.at(i++).toInt();
    int last = args.at(i++).toInt();
    emit q->headerDataChanged(orientation, first, last);
}

void QRemoteModelClient::Private::layoutChanged(const QVariantList &args)
{
    Q_UNUSED(args)
    emit q->layoutChanged();
//    Q_UNREACHABLE();
}

void QRemoteModelClient::Private::layoutAboutToBeChanged(const QVariantList &args)
{
    Q_UNUSED(args)
    emit q->layoutAboutToBeChanged();
//    Q_UNREACHABLE();
}

void QRemoteModelClient::Private::rowsAboutToBeInserted(const QVariantList &args)
{
    int i = 0;
    QModelIndex parent = QtRemoteModel::toModelIndex(q, args.at(i++));
    int first = args.at(i++).toInt();
    int last = args.at(i++).toInt();
    q->beginInsertRows(parent, first, last);
}

void QRemoteModelClient::Private::rowsInserted(const QVariantList &args)
{
    int i = 0;
    QModelIndex parent = QtRemoteModel::toModelIndex(q, args.at(i++));
    int first = args.at(i++).toInt();
    int last = args.at(i++).toInt();
    int count = last - first + 1;
    Node *parentNode = parent.internalPointer() ? static_cast<Node *>(parent.internalPointer()) : rootNode;
    int columnCount = 0;
    if (parentNode->children.isEmpty()) {
        columnCount = methodCall("columnCount", QVariantList() << QtRemoteModel::fromModelIndex(parent)).toInt();
    } else {
        columnCount = q->columnCount(parent);
        foreach (Node *child, parentNode->children) {
            if (child->row > first - 1) {
                child->row += count;
            }
        }
    }
    for (int row = first; row <= last; row++) {
        for (int column = 0; column < columnCount; column++) {
            new Node(row, column, parentNode);
        }
    }
    parentNode->check(Q_FUNC_INFO, __LINE__);
    q->endInsertRows();
}

void QRemoteModelClient::Private::rowsAboutToBeMoved(const QVariantList &args)
{
    int i = 0;
    QModelIndex sourceParent = QtRemoteModel::toModelIndex(q, args.at(i++));
    int sourceFirst = args.at(i++).toInt();
    int sourceLast = args.at(i++).toInt();
    QModelIndex destinationParent = QtRemoteModel::toModelIndex(q, args.at(i++));
    int destinationRow = args.at(i++).toInt();
    q->beginMoveRows(sourceParent, sourceFirst, sourceLast, destinationParent, destinationRow);
}

void QRemoteModelClient::Private::rowsMoved(const QVariantList &args)
{
    int i = 0;
    QModelIndex sourceParent = QtRemoteModel::toModelIndex(q, args.at(i++));
    int sourceFirst = args.at(i++).toInt();
    int sourceLast = args.at(i++).toInt();
    int count = sourceLast - sourceFirst + 1;
    QModelIndex destinationParent = QtRemoteModel::toModelIndex(q, args.at(i++));
    int destinationRow = args.at(i++).toInt();

    QList<Node *> nodes;
    Node *parentNode = sourceParent.internalPointer() ? static_cast<Node *>(sourceParent.internalPointer()) : rootNode;
    QMutableListIterator<Node *> it(parentNode->children);
    while (it.hasNext()) {
        Node *child = it.next();
        if (child->row < sourceFirst) {
            // nothing to do
        } else if (child->row < sourceLast + 1) {
            it.remove();
            child->row = destinationRow + child->row - sourceFirst - count;
            nodes.append(child);
        } else {
            child->row -= count;
        }
    }

    parentNode = destinationParent.internalPointer() ? static_cast<Node *>(destinationParent.internalPointer()) : rootNode;
    foreach (Node *node, parentNode->children) {
        if (node->row > destinationRow - 1 - count) {
            node->row += count;
        }
    }
    parentNode->children.append(nodes);
    parentNode->check(Q_FUNC_INFO, __LINE__);

    qDebug() << sourceParent << sourceFirst << sourceLast << destinationParent << destinationRow;
    q->endMoveRows();
}

void QRemoteModelClient::Private::rowsAboutToBeRemoved(const QVariantList &args)
{
    int i = 0;
    QModelIndex parent = QtRemoteModel::toModelIndex(q, args.at(i++));
    int first = args.at(i++).toInt();
    int last = args.at(i++).toInt();
    q->beginRemoveRows(parent, first, last);
}

void QRemoteModelClient::Private::rowsRemoved(const QVariantList &args)
{
    int i = 0;
    QModelIndex parent = QtRemoteModel::toModelIndex(q, args.at(i++));
    int first = args.at(i++).toInt();
    int last = args.at(i++).toInt();
    int count = last - first + 1;
    Node *parentNode = parent.internalPointer() ? static_cast<Node *>(parent.internalPointer()) : rootNode;
    QMutableListIterator<Node *> it(parentNode->children);
    while (it.hasNext()) {
        Node *child = it.next();
        if (child->row < first) {
            // nothing to do
        } else if (child->row < last + 1) {
            it.remove();
            delete child;
        } else {
            child->row -= count;
        }
    }
    parentNode->check(Q_FUNC_INFO, __LINE__);
    q->endRemoveRows();
}

void QRemoteModelClient::Private::columnsAboutToBeInserted(const QVariantList &args)
{
    int i = 0;
    QModelIndex parent = QtRemoteModel::toModelIndex(q, args.at(i++));
    int first = args.at(i++).toInt();
    int last = args.at(i++).toInt();
    q->beginInsertColumns(parent, first, last);
}

void QRemoteModelClient::Private::columnsInserted(const QVariantList &args)
{
    Q_UNUSED(args)
    q->endInsertColumns();
}

void QRemoteModelClient::Private::columnsAboutToBeMoved(const QVariantList &args)
{
    int i = 0;
    QModelIndex sourceParent = QtRemoteModel::toModelIndex(q, args.at(i++));
    int sourceFirst = args.at(i++).toInt();
    int sourceLast = args.at(i++).toInt();
    QModelIndex destinationParent = QtRemoteModel::toModelIndex(q, args.at(i++));
    int destinationColumn = args.at(i++).toInt();
    q->beginMoveColumns(sourceParent, sourceFirst, sourceLast, destinationParent, destinationColumn);
}

void QRemoteModelClient::Private::columnsMoved(const QVariantList &args)
{
    Q_UNUSED(args)
    q->endMoveColumns();
}

void QRemoteModelClient::Private::columnsAboutToBeRemoved(const QVariantList &args)
{
    int i = 0;
    QModelIndex parent = QtRemoteModel::toModelIndex(q, args.at(i++));
    int first = args.at(i++).toInt();
    int last = args.at(i++).toInt();
    q->beginRemoveColumns(parent, first, last);
}

void QRemoteModelClient::Private::columnsRemoved(const QVariantList &args)
{
    Q_UNUSED(args)
    q->endRemoveColumns();
}

void QRemoteModelClient::Private::modelAboutToBeReset(const QVariantList &args)
{
    Q_UNUSED(args)
    q->beginResetModel();
}

void QRemoteModelClient::Private::modelReset(const QVariantList &args)
{
    Q_UNUSED(args)
    q->endResetModel();
}

QRemoteModelClient::QRemoteModelClient(QObject *parent)
    : QAbstractItemModel(parent)
    , d(new Private(this))
{
}

QRemoteModelClient::~QRemoteModelClient()
{
    delete d;
}

void QRemoteModelClient::connectToHost(const QHostAddress &address, quint16 port)
{
    d->connectToHost(address, port);
    d->waitForConnected();
}

QModelIndex QRemoteModelClient::index(int row, int column, const QModelIndex &parent) const
{
    QModelIndex ret;
    if (hasIndex(row, column, parent)) {
        Node *parentNode = parent.internalPointer() ? static_cast<Node *>(parent.internalPointer()) : d->rootNode;
        foreach (Node *childNode, parentNode->children) {
            if (childNode->row == row && childNode->column == column) {
                ret = createIndex(row, column, childNode);
                break;
            }
        }
    }
    return ret;
}

QModelIndex QRemoteModelClient::parent(const QModelIndex &child) const
{
    QModelIndex ret;
    if (child.isValid() && child.internalPointer()) {
        Node *parentNode = static_cast<Node *>(child.internalPointer())->parent;
        ret = createIndex(parentNode->row, parentNode->column, parentNode);
    }
    return ret;
}

QModelIndex QRemoteModelClient::sibling(int row, int column, const QModelIndex &idx) const
{
    QModelIndex ret = QAbstractItemModel::sibling(row, column, idx);
    return ret;
}

int QRemoteModelClient::rowCount(const QModelIndex &parent) const
{
    int ret = 0;
    const Node *node = parent.internalPointer() ? static_cast<const Node *>(parent.internalPointer()) : d->rootNode;
    foreach (const Node *child, node->children) {
        ret = std::max(ret, child->row + 1);
    }
    qDebug() << parent << ret;
    return ret;
}

int QRemoteModelClient::columnCount(const QModelIndex &parent) const
{
    int ret = 0;
    Node *node = parent.internalPointer() ? static_cast<Node *>(parent.internalPointer()) : d->rootNode;
    foreach (const Node *child, node->children) {
        ret = std::max(ret, child->column + 1);
    }
    qDebug() << parent << ret;
    return ret;
}

bool QRemoteModelClient::hasChildren(const QModelIndex &parent) const
{
    Node *node = parent.internalPointer() ? static_cast<Node *>(parent.internalPointer()) : d->rootNode;
    bool ret = !node->children.isEmpty();
//    qDebug() << parent << ret;
    return ret;
}

QVariant QRemoteModelClient::data(const QModelIndex &index, int role) const
{
    return d->methodCall("data", QVariantList() << QtRemoteModel::fromModelIndex(index) << role);
}

QVariant QRemoteModelClient::headerData(int section, Qt::Orientation orientation, int role) const
{
    return d->methodCall("headerData", QVariantList() << section << orientation << role);
}

QMap<int, QVariant> QRemoteModelClient::itemData(const QModelIndex &index) const
{
    QMap<int,QVariant> ret;
    QMap<QString, QVariant> hash = d->methodCall("itemData", QVariantList() << QtRemoteModel::fromModelIndex(index)).toMap();
    QMapIterator<QString, QVariant> i(hash);
    while (i.hasNext()) {
        i.next();
        ret.insert(i.key().toInt(), i.value());
    }
    return ret;
}

void QRemoteModelClient::fetchMore(const QModelIndex &parent)
{
    d->methodCall("fetchMore", QVariantList() << QtRemoteModel::fromModelIndex(parent));
}

bool QRemoteModelClient::canFetchMore(const QModelIndex &parent) const
{
    return d->methodCall("canFetchMore", QVariantList() << QtRemoteModel::fromModelIndex(parent)).toBool();
}

Qt::ItemFlags QRemoteModelClient::flags(const QModelIndex &index) const
{
    return static_cast<Qt::ItemFlags>(d->methodCall("flags", QVariantList() << QtRemoteModel::fromModelIndex(index)).toInt());
}

QHash<int,QByteArray> QRemoteModelClient::roleNames() const
{
    QHash<int,QByteArray> ret;
    QHash<QString, QVariant> hash = d->methodCall("roleNames").toHash();
    QHashIterator<QString, QVariant> i(hash);
    while (i.hasNext()) {
        i.next();
        ret.insert(i.key().toInt(), i.value().toByteArray());
    }
    return ret;
}

#include "qremotemodelclient.moc"
