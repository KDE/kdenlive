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
#include <QDebug>
#include <utility>

int AbstractTreeModel::currentTreeId = 0;
AbstractTreeModel::AbstractTreeModel(QObject *parent)
    : QAbstractItemModel(parent)
{
}

std::shared_ptr<AbstractTreeModel> AbstractTreeModel::construct(QObject *parent)
{
    std::shared_ptr<AbstractTreeModel> self(new AbstractTreeModel(parent));
    self->rootItem.reset(new TreeItem(QList<QVariant>(), self));
    return self;
}

AbstractTreeModel::~AbstractTreeModel() = default;

int AbstractTreeModel::columnCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) return rootItem->columnCount();

    const auto id = (int)parent.internalId();
    auto item = getItemById(id);
    return item->columnCount();
}

QVariant AbstractTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    if (role != Qt::DisplayRole) {
        return QVariant();
    }
    auto item = getItemById((int)index.internalId());
    return item->dataColumn(index.column());
}

Qt::ItemFlags AbstractTreeModel::flags(const QModelIndex &index) const
{
    const auto flags = QAbstractItemModel::flags(index);

    if (index.isValid()) {
        auto item = getItemById((int)index.internalId());
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

    std::shared_ptr<TreeItem> parentItem;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = getItemById((int)parent.internalId());

    std::shared_ptr<TreeItem> childItem = parentItem->child(row);
    if (childItem) return createIndex(row, column, quintptr(childItem->getId()));

    return QModelIndex();
}

QModelIndex AbstractTreeModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) return QModelIndex();

    std::shared_ptr<TreeItem> childItem = getItemById((int)index.internalId());
    std::shared_ptr<TreeItem> parentItem = childItem->parentItem().lock();

    Q_ASSERT(parentItem);

    if (parentItem == rootItem) return QModelIndex();

    return createIndex(parentItem->row(), 0, quintptr(parentItem->getId()));
}

int AbstractTreeModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0) return 0;

    std::shared_ptr<TreeItem> parentItem;
    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = getItemById((int)parent.internalId());

    return parentItem->childCount();
}

QModelIndex AbstractTreeModel::getIndexFromItem(const std::shared_ptr<TreeItem> &item) const
{
    if (item == rootItem) {
        return QModelIndex();
    }
    return index(item->row(), 0, getIndexFromItem(item->parentItem().lock()));
}

void AbstractTreeModel::notifyRowAboutToAppend(const std::shared_ptr<TreeItem> &item)
{
    auto index = getIndexFromItem(item);
    beginInsertRows(index, item->childCount(), item->childCount());
}

void AbstractTreeModel::notifyRowAppended(const std::shared_ptr<TreeItem> &row)
{
    Q_UNUSED(row);
    endInsertRows();
}

void AbstractTreeModel::notifyRowAboutToDelete(std::shared_ptr<TreeItem> item, int row)
{
    auto index = getIndexFromItem(std::move(item));
    beginRemoveRows(index, row, row);
}

void AbstractTreeModel::notifyRowDeleted()
{
    endRemoveRows();
}

// static
int AbstractTreeModel::getNextId()
{
    return currentTreeId++;
}

void AbstractTreeModel::registerItem(const std::shared_ptr<TreeItem> &item)
{
    int id = item->getId();
    Q_ASSERT(m_allItems.count(id) == 0);
    m_allItems[id] = item;
}

void AbstractTreeModel::deregisterItem(int id, TreeItem *item)
{
    Q_UNUSED(item);
    Q_ASSERT(m_allItems.count(id) > 0);
    m_allItems.erase(id);
}

std::shared_ptr<TreeItem> AbstractTreeModel::getItemById(int id) const
{
    if (id == rootItem->getId()) {
        return rootItem;
    }
    Q_ASSERT(m_allItems.count(id) > 0);
    return m_allItems.at(id).lock();
}

std::shared_ptr<TreeItem> AbstractTreeModel::getRoot() const
{
    return rootItem;
}
