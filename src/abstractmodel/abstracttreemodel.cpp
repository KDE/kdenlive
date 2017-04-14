/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#include "abstracttreemodel.hpp"
#include "treeitem.hpp"

AbstractTreeModel::AbstractTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    rootItem = new TreeItem(QList<QVariant>(), this);
}

AbstractTreeModel::~AbstractTreeModel()
{
    delete rootItem;
}

int AbstractTreeModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid()) return static_cast<TreeItem *>(parent.internalPointer())->columnCount();

    return rootItem->columnCount();
}

QVariant AbstractTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (role != Qt::DisplayRole) {
        return QVariant();
    }
    TreeItem *item = static_cast<TreeItem *>(index.internalPointer());
    return item->dataColumn(index.column());
}

Qt::ItemFlags AbstractTreeModel::flags(const QModelIndex &index) const
{
    const auto flags = QAbstractItemModel::flags(index);

    if (index.isValid()) {
        TreeItem *item = static_cast<TreeItem *>(index.internalPointer());
        if (item->depth() == 1) {
            return flags & ~Qt::ItemIsSelectable;
        }
    }
    return flags;
}

QVariant AbstractTreeModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) return rootItem->dataColumn(section);

    return QVariant();
}

QModelIndex AbstractTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) return QModelIndex();

    TreeItem *parentItem;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<TreeItem *>(parent.internalPointer());

    TreeItem *childItem = parentItem->child(row);
    if (childItem) return createIndex(row, column, childItem);

    return QModelIndex();
}

QModelIndex AbstractTreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) return QModelIndex();

    TreeItem *childItem = static_cast<TreeItem *>(index.internalPointer());
    TreeItem *parentItem = childItem->parentItem();

    if (parentItem == rootItem) return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

int AbstractTreeModel::rowCount(const QModelIndex &parent) const
{
    TreeItem *parentItem;
    if (parent.column() > 0) return 0;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<TreeItem *>(parent.internalPointer());

    return parentItem->childCount();
}

QModelIndex AbstractTreeModel::getIndexFromItem(TreeItem *item) const
{
    if (item == rootItem) {
        return QModelIndex();
    }
    return index(item->row(), 0, getIndexFromItem(item->parentItem()));
}

void AbstractTreeModel::notifyRowAboutToAppend(TreeItem *item)
{
    auto index = getIndexFromItem(item);
    beginInsertRows(index, item->childCount(), item->childCount());
}

void AbstractTreeModel::notifyRowAppended()
{
    endInsertRows();
}

void AbstractTreeModel::notifyRowAboutToDelete(TreeItem *item, int row)
{
    auto index = getIndexFromItem(item);
    beginRemoveRows(index, row, row);
}

void AbstractTreeModel::notifyRowDeleted()
{
    endRemoveRows();
}
