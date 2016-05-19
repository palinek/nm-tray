/*COPYRIGHT_HEADER

This file is a part of nm-tray.

Copyright (c)
    2015~now Palo Kisa <palo.kisa@gmail.com>

nm-tray is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

COPYRIGHT_HEADER*/
#include "nmproxy.h"
#include "log.h"

void NmProxy::setSourceModel(QAbstractItemModel * sourceModel)
{
    //we operate only on our known model
    Q_ASSERT(sourceModel->metaObject() == &NmModel::staticMetaObject);
    Q_ASSERT(root.isValid());
    beginResetModel();

    if (nullptr != this->sourceModel())
    {
        disconnect(this->sourceModel(), &QAbstractItemModel::dataChanged, this, &NmProxy::onSourceDataChanged);
        disconnect(this->sourceModel(), &QAbstractItemModel::headerDataChanged, this, &NmProxy::onSourceHeaderDataChanged);
        disconnect(this->sourceModel(), &QAbstractItemModel::rowsAboutToBeInserted, this, &NmProxy::onSourceRowsAboutToBeInserted);
        disconnect(this->sourceModel(), &QAbstractItemModel::rowsInserted, this, &NmProxy::onSourceRowsInserted);
        disconnect(this->sourceModel(), &QAbstractItemModel::rowsAboutToBeRemoved, this, &NmProxy::onSourceRowsAboutToBeRemoved);
        disconnect(this->sourceModel(), &QAbstractItemModel::rowsRemoved, this, &NmProxy::onSourceRowsRemoved);
        disconnect(this->sourceModel(), &QAbstractItemModel::columnsAboutToBeInserted, this, &NmProxy::onSourceColumnsAboutToBeInserted);
        disconnect(this->sourceModel(), &QAbstractItemModel::columnsInserted, this, &NmProxy::onSourceColumnsInserted);
        disconnect(this->sourceModel(), &QAbstractItemModel::columnsAboutToBeRemoved, this, &NmProxy::onSourceColumnsAboutToBeRemoved);
        disconnect(this->sourceModel(), &QAbstractItemModel::columnsRemoved, this, &NmProxy::onSourceColumnsRemoved);
        disconnect(this->sourceModel(), &QAbstractItemModel::modelAboutToBeReset, this, &NmProxy::onSourceModelAboutToBeReset);
        disconnect(this->sourceModel(), &QAbstractItemModel::modelReset, this, &NmProxy::onSourceModelReset);
        disconnect(this->sourceModel(), &QAbstractItemModel::rowsAboutToBeMoved, this, &NmProxy::onSourceRowsAboutToBeMoved);
        disconnect(this->sourceModel(), &QAbstractItemModel::rowsMoved, this, &NmProxy::onSourceRowsMoved);
        disconnect(this->sourceModel(), &QAbstractItemModel::columnsAboutToBeMoved, this, &NmProxy::onSourceColumnsAboutToBeMoved);
        disconnect(this->sourceModel(), &QAbstractItemModel::columnsMoved, this, &NmProxy::onSourceColumnsMoved);
    }

    QAbstractProxyModel::setSourceModel(sourceModel);

    connect(sourceModel, &QAbstractItemModel::dataChanged, this, &NmProxy::onSourceDataChanged);
    connect(sourceModel, &QAbstractItemModel::headerDataChanged, this, &NmProxy::onSourceHeaderDataChanged);
    connect(sourceModel, &QAbstractItemModel::rowsAboutToBeInserted, this, &NmProxy::onSourceRowsAboutToBeInserted);
    connect(sourceModel, &QAbstractItemModel::rowsInserted, this, &NmProxy::onSourceRowsInserted);
    connect(sourceModel, &QAbstractItemModel::rowsAboutToBeRemoved, this, &NmProxy::onSourceRowsAboutToBeRemoved);
    connect(sourceModel, &QAbstractItemModel::rowsRemoved, this, &NmProxy::onSourceRowsRemoved);
    connect(sourceModel, &QAbstractItemModel::columnsAboutToBeInserted, this, &NmProxy::onSourceColumnsAboutToBeInserted);
    connect(sourceModel, &QAbstractItemModel::columnsInserted, this, &NmProxy::onSourceColumnsInserted);
    connect(sourceModel, &QAbstractItemModel::columnsAboutToBeRemoved, this, &NmProxy::onSourceColumnsAboutToBeRemoved);
    connect(sourceModel, &QAbstractItemModel::columnsRemoved, this, &NmProxy::onSourceColumnsRemoved);
    connect(sourceModel, &QAbstractItemModel::modelAboutToBeReset, this, &NmProxy::onSourceModelAboutToBeReset);
    connect(sourceModel, &QAbstractItemModel::modelReset, this, &NmProxy::onSourceModelReset);
    connect(sourceModel, &QAbstractItemModel::rowsAboutToBeMoved, this, &NmProxy::onSourceRowsAboutToBeMoved);
    connect(sourceModel, &QAbstractItemModel::rowsMoved, this, &NmProxy::onSourceRowsMoved);
    connect(sourceModel, &QAbstractItemModel::columnsAboutToBeMoved, this, &NmProxy::onSourceColumnsAboutToBeMoved);
    connect(sourceModel, &QAbstractItemModel::columnsMoved, this, &NmProxy::onSourceColumnsMoved);

    endResetModel();
}

void NmProxy::setNmModel(NmModel * model, NmModel::ItemType shownType)
{
    root = model->indexTypeRoot(shownType);
    setSourceModel(model);
//qCDebug(NM_TRAY) << __FUNCTION__ << model->indexTypeRoot(shownType) << "->" << root;
}

QModelIndex NmProxy::index(int row, int column, const QModelIndex & proxyParent) const
{
    QModelIndex i;
    if (hasIndex(row, column, proxyParent))
        i = createIndex(row, column);
//qCDebug(NM_TRAY) << __FUNCTION__ << row << column << proxyParent << "->" << i;
    return i;
}

QModelIndex NmProxy::parent(const QModelIndex &/*proxyIndex*/) const
{
    //showing only one level/list: leaf -> invalid, root -> invalid
    QModelIndex i;
//qCDebug(NM_TRAY) << __FUNCTION__ << proxyIndex << "->" << i;
    return i;
}

int NmProxy::rowCount(const QModelIndex &proxyParent) const
{
    //showing only one level/list: leaf -> source root rowcount, root -> 0
    if (proxyParent.isValid())
        return 0;
    else
        return sourceModel()->rowCount(root);
}

int NmProxy::columnCount(const QModelIndex &proxyParent) const
{
    return sourceModel()->columnCount(mapToSource(proxyParent));
}

void NmProxy::activateConnection(QModelIndex const & index) const
{
    qobject_cast<NmModel *>(sourceModel())->activateConnection(mapToSource(index));
}

void NmProxy::deactivateConnection(QModelIndex const & index) const
{
    qobject_cast<NmModel *>(sourceModel())->deactivateConnection(mapToSource(index));
}

QModelIndex NmProxy::mapToSource(const QModelIndex & proxyIndex) const
{
    QModelIndex i;
    if (proxyIndex.isValid())
        i = sourceModel()->index(proxyIndex.row(), proxyIndex.column(), root);
//qCDebug(NM_TRAY) << __FUNCTION__ << proxyIndex << "->" << i;
    return i;
}

QModelIndex NmProxy::mapFromSource(const QModelIndex & sourceIndex) const
{
    QModelIndex i;
    if (sourceIndex.isValid() && root != sourceIndex)
        i = createIndex(sourceIndex.row(), sourceIndex.column());
//qCDebug(NM_TRAY) << __FUNCTION__ << sourceIndex << "->" << i;
    return i;
}

void NmProxy::onSourceDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles)
{
    if (root == topLeft.parent())
        emit dataChanged(mapFromSource(topLeft), mapFromSource(bottomRight), roles);
}

void NmProxy::onSourceHeaderDataChanged(Qt::Orientation orientation, int first, int last)
{
    emit headerDataChanged(orientation, first, last);
}

void NmProxy::onSourceRowsAboutToBeInserted(const QModelIndex &parent, int first, int last)
{
    if (root == parent)
        beginInsertRows(mapFromSource(parent), first, last);
}

void NmProxy::onSourceRowsInserted(const QModelIndex &parent, int, int)
{
    if (root == parent)
        endInsertRows();
}

void NmProxy::onSourceRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last)
{
    if (root == parent)
        beginRemoveRows(mapFromSource(parent), first, last);
}

void NmProxy::onSourceRowsRemoved(const QModelIndex &parent, int, int)
{
    if (root == parent)
        endRemoveRows();
}

void NmProxy::onSourceColumnsAboutToBeInserted(const QModelIndex &parent, int first, int last)
{
    if (root == parent)
        beginInsertColumns(mapFromSource(parent), first, last);
}

void NmProxy::onSourceColumnsInserted(const QModelIndex &parent, int, int)
{
    if (root == parent)
        endInsertColumns();
}


void NmProxy::onSourceColumnsAboutToBeRemoved(const QModelIndex &parent, int first, int last)
{
    if (root == parent)
        beginRemoveColumns(mapFromSource(parent), first, last);
}

void NmProxy::onSourceColumnsRemoved(const QModelIndex &parent, int, int)
{
    if (root == parent)
        endRemoveColumns();
}

void NmProxy::onSourceModelAboutToBeReset()
{
    beginResetModel();
}

void NmProxy::onSourceModelReset()
{
    endResetModel();
}

void NmProxy::onSourceRowsAboutToBeMoved( const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destinationParent, int destinationRow)
{
    if (root == sourceParent)
    {
        if (root == destinationParent)
            beginMoveRows(mapFromSource(sourceParent), sourceStart, sourceEnd, destinationParent, destinationRow);
        else
            beginRemoveRows(mapFromSource(sourceParent), sourceStart, sourceEnd);
    } else if (root == destinationParent)
        beginInsertRows(mapFromSource(destinationParent), destinationRow, destinationRow + (sourceEnd - sourceStart));
}

void NmProxy::onSourceRowsMoved( const QModelIndex &sourceParent, int, int, const QModelIndex &destinationParent, int)
{
    if (root == sourceParent)
    {
        if (root == destinationParent)
            endMoveRows();
        else
            endRemoveRows();
    } else if (root == destinationParent)
        endInsertRows();
}


void NmProxy::onSourceColumnsAboutToBeMoved( const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destinationParent, int destinationColumn)
{
    if (root == sourceParent)
    {
        if (root == destinationParent)
            beginMoveColumns(mapFromSource(sourceParent), sourceStart, sourceEnd, destinationParent, destinationColumn);
        else
            beginRemoveColumns(mapFromSource(sourceParent), sourceStart, sourceEnd);
    } else if (root == destinationParent)
        beginInsertColumns(mapFromSource(destinationParent), destinationColumn, destinationColumn + (sourceEnd - sourceStart));
}

void NmProxy::onSourceColumnsMoved( const QModelIndex &sourceParent, int, int, const QModelIndex &destinationParent, int)
{
    if (root == sourceParent)
    {
        if (root == destinationParent)
            endMoveColumns();
        else
            endRemoveColumns();
    } else if (root == destinationParent)
        endInsertColumns();
}

