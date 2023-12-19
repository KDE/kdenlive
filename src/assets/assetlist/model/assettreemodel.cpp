/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "assettreemodel.hpp"
#include "abstractmodel/treeitem.hpp"
#include "effects/effectsrepository.hpp"
#include "transitions/transitionsrepository.hpp"
#include <QIcon>

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

Qt::ItemFlags AssetTreeModel::flags(const QModelIndex &index) const
{
    if (!index.isValid()) return Qt::ItemFlags();
    std::shared_ptr<TreeItem> item = getItemById(int(index.internalId()));
    if (item->dataColumn(AssetTreeModel::IdCol) == QStringLiteral("root")) {
        return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
    }
    return Qt::ItemIsDragEnabled | Qt::ItemIsEnabled | Qt::ItemIsSelectable;
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
    case Qt::DecorationRole: {
        if (item->dataColumn(AssetTreeModel::IdCol).toString() == QLatin1String("root") || item->dataColumn(0).toString().isEmpty()) {
            return QIcon();
        }
        if (auto pt = item->parentItem().lock()) {
            return QIcon(m_assetIconProvider->makePixmap(pt->dataColumn(0).toString() + QLatin1String("/") +
                                                         QString::number(item->dataColumn(AssetTreeModel::TypeCol).toInt()) + QLatin1String("/") +
                                                         item->dataColumn(0).toString().at(0).toUpper()));
        }
        return QIcon();
    }
    default:
        return QVariant();
    }
}
