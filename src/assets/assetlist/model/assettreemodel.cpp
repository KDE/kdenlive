/*
 *   SPDX-FileCopyrightText: 2017 Nicolas Carion *
 * SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
 */

#include "assettreemodel.hpp"
#include "abstractmodel/treeitem.hpp"
#include "effects/effectsrepository.hpp"
#include "transitions/transitionsrepository.hpp"

int AssetTreeModel::nameCol = 0;
int AssetTreeModel::idCol = 1;
int AssetTreeModel::typeCol = 2;
int AssetTreeModel::favCol = 3;
int AssetTreeModel::preferredCol = 5;

AssetTreeModel::AssetTreeModel(QObject *parent)
    : AbstractTreeModel(parent)
{
}

QHash<int, QByteArray> AssetTreeModel::roleNames() const
{
    QHash<int, QByteArray> roles;
    roles[IdRole] = "identifier";
    roles[NameRole] = "name";
    roles[FavoriteRole] = "favorite";
    roles[TypeRole] = "type";
    return roles;
}

QString AssetTreeModel::getName(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QString();
    }
    std::shared_ptr<TreeItem> item = getItemById(int(index.internalId()));
    if (item->depth() == 1) {
        return item->dataColumn(0).toString();
    }
    return item->dataColumn(AssetTreeModel::nameCol).toString();
}

bool AssetTreeModel::isFavorite(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return false;
    }
    std::shared_ptr<TreeItem> item = getItemById(int(index.internalId()));
    if (item->depth() == 1) {
        return false;
    }
    return item->dataColumn(AssetTreeModel::favCol).toBool();
}

QString AssetTreeModel::getDescription(bool isEffect, const QModelIndex &index) const
{
    if (!index.isValid()) {
        return QString();
    }
    std::shared_ptr<TreeItem> item = getItemById(int(index.internalId()));
    if (isEffect && item->depth() == 1) {
        return QString();
    }
    auto id = item->dataColumn(AssetTreeModel::idCol).toString();
    if (isEffect && EffectsRepository::get()->exists(id)) {
        return EffectsRepository::get()->getDescription(id);
    }
    if (!isEffect && TransitionsRepository::get()->exists(id)) {
        return TransitionsRepository::get()->getDescription(id);
    }
    return QString();
}


QVariant AssetTreeModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid()) {
        return QVariant();
    }

    std::shared_ptr<TreeItem> item = getItemById(int(index.internalId()));
    switch (role) {
    case IdRole:
        return item->dataColumn(AssetTreeModel::idCol);
    case FavoriteRole:
        return item->dataColumn(AssetTreeModel::favCol);
    case TypeRole:
        return item->dataColumn(AssetTreeModel::typeCol);
    case NameRole:
    case Qt::DisplayRole:
        return item->dataColumn(index.column());
    default:
        return QVariant();
    }
}
