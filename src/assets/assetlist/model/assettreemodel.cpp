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

#include "assettreemodel.hpp"
#include "effects/effectsrepository.hpp"
#include "transitions/transitionsrepository.hpp"
#include "abstractmodel/treeitem.hpp"

int AssetTreeModel::nameCol = 0;
int AssetTreeModel::idCol = 1;
int AssetTreeModel::typeCol = 2;
int AssetTreeModel::favCol = 3;

AssetTreeModel::AssetTreeModel(QObject *parent)
    : AbstractTreeModel(parent)
{
}


QHash<int, QByteArray> AssetTreeModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "identifier";
    roles[NameRole] = "name";
    return roles;
}

QString AssetTreeModel::getName(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return QString();
    }
    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
    if (item->depth() == 1) {
        return item->data(0).toString();
    } else {
        return item->data(AssetTreeModel::nameCol).toString();
    }
}

QString AssetTreeModel::getDescription(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return QString();
    }
    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
    if (item->depth() == 1) {
        return QString();
    } else {
        auto id = item->data(AssetTreeModel::idCol).toString();
        if (EffectsRepository::get()->exists(id)){
            return EffectsRepository::get()->getDescription(id);
        }
        if (TransitionsRepository::get()->exists(id)){
            return TransitionsRepository::get()->getDescription(id);
        }
        return QString();
    }
}


QVariant AssetTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    TreeItem *item = static_cast<TreeItem*>(index.internalPointer());
    if(role == IdRole) {
        return item->data(AssetTreeModel::idCol);
    }

    if (role != NameRole) {
        return QVariant();
    }
    return item->data(index.column());
}

QList <QModelIndex> AssetTreeModel::getChildrenIndexes()
{
    QList <QModelIndex> indexes;
    for(int i = 0; i != rootItem->childCount(); ++i) {
        TreeItem *child = rootItem->child(i);
        indexes << createIndex(i,0, child);
    }

    return indexes;
}
