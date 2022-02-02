/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "assettreemodel.hpp"
#include "abstractmodel/treeitem.hpp"
#include "effects/effectsrepository.hpp"
#include "transitions/transitionsrepository.hpp"

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
    return item->dataColumn(AssetTreeModel::NameCol).toString();
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
    return item->dataColumn(AssetTreeModel::FavCol).toBool();
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
    auto id = item->dataColumn(AssetTreeModel::IdCol).toString();
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
        return item->dataColumn(AssetTreeModel::IdCol);
    case FavoriteRole:
        return item->dataColumn(AssetTreeModel::FavCol);
    case TypeRole:
        return item->dataColumn(AssetTreeModel::TypeCol);
    case NameRole:
    case Qt::DisplayRole:
        return item->dataColumn(index.column());
    default:
        return QVariant();
    }
}
