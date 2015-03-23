#include "qtremotemodel_global.h"

#include <QtCore/QPoint>

QVariant QtRemoteModel::fromModelIndex(const QModelIndex &index) {
    QVariantList ret;
    for (QModelIndex i = index; i.isValid(); i = i.parent()) {
//        qDebug() << i;
        QPoint point;
        point.setX(i.column());
        point.setY(i.row());
        ret.prepend(point);
    }
    return ret;
}

QModelIndex QtRemoteModel::toModelIndex(const QAbstractItemModel *model, const QVariant &value) {
    QModelIndex ret;
    QVariantList list = value.toList();
    foreach (const QVariant &value, list) {
        QPoint point = value.toPoint();
        int row = point.y();
        int column = point.x();
        ret = model->index(row, column, ret);
    }
    return ret;
}

QVariant QtRemoteModel::toVariant(const QVector<int> source) {
    QVariantList ret;
    foreach (int value, source) {
        ret.append(value);
    }
    return ret;
}
