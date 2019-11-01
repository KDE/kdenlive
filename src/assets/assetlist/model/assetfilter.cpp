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

#include "assetfilter.hpp"

#include "abstractmodel/abstracttreemodel.hpp"
#include "abstractmodel/treeitem.hpp"
#include "assettreemodel.hpp"
#include <utility>
#include <klocalizedstring.h>

AssetFilter::AssetFilter(QObject *parent)
    : QSortFilterProxyModel(parent)

{
    setFilterRole(Qt::DisplayRole);
    setSortRole(Qt::DisplayRole);
    setDynamicSortFilter(false);
}

void AssetFilter::setFilterName(bool enabled, const QString &pattern)
{
    m_name_enabled = enabled;
    m_name_value = pattern;
    invalidateFilter();
    if (rowCount() > 1) {
        sort(0);
    }
}

bool AssetFilter::lessThan(const QModelIndex &left,
                                      const QModelIndex &right) const
{
    QString leftData = sourceModel()->data(left).toString();
    QString rightData = sourceModel()->data(right).toString();
    return QString::localeAwareCompare(leftData, rightData) < 0;
}

bool AssetFilter::filterName(const std::shared_ptr<TreeItem> &item) const
{
    if (!m_name_enabled) {
        return true;
    }
    QString itemText = i18n(item->dataColumn(AssetTreeModel::nameCol).toString().toUtf8().constData());
    itemText = itemText.normalized(QString::NormalizationForm_D).remove(QRegExp(QStringLiteral("[^a-zA-Z0-9\\s]")));
    QString patt = m_name_value.normalized(QString::NormalizationForm_D).remove(QRegExp(QStringLiteral("[^a-zA-Z0-9\\s]")));

    return itemText.contains(patt, Qt::CaseInsensitive);
}

bool AssetFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex row = sourceModel()->index(sourceRow, 0, sourceParent);
    auto *model = static_cast<AbstractTreeModel *>(sourceModel());
    std::shared_ptr<TreeItem> item = model->getItemById((int)row.internalId());

    if (item->dataColumn(AssetTreeModel::idCol) == QStringLiteral("root")) {
        // In that case, we have a category. We hide it if it does not have children.
        QModelIndex category = sourceModel()->index(sourceRow, 0, sourceParent);
        if (!category.isValid()) {
            return false;
        }
        bool accepted = false;
        for (int i = 0; i < sourceModel()->rowCount(category) && !accepted; ++i) {
            accepted = filterAcceptsRow(i, category);
        }
        return accepted;
    }
    return applyAll(item);
}

bool AssetFilter::isVisible(const QModelIndex &sourceIndex)
{
    auto parent = sourceModel()->parent(sourceIndex);
    return filterAcceptsRow(sourceIndex.row(), parent);
}

bool AssetFilter::applyAll(std::shared_ptr<TreeItem> item) const
{
    return filterName(item);
}

QModelIndex AssetFilter::getNextChild(const QModelIndex &current)
{
    QModelIndex nextItem = current.sibling(current.row() + 1, current.column());
    if (!nextItem.isValid()) {
        QModelIndex folder = index(current.parent().row() + 1, 0, QModelIndex());
        if (!folder.isValid()) {
            return current;
        }
        while (folder.isValid() && rowCount(folder) == 0) {
            folder = folder.sibling(folder.row() + 1, folder.column());
        }
        if (folder.isValid() && rowCount(folder) > 0) {
            return index(0, current.column(), folder);
        }
        nextItem = current;
    }
    return nextItem;
}

QModelIndex AssetFilter::getPreviousChild(const QModelIndex &current)
{
    QModelIndex nextItem = current.sibling(current.row() - 1, current.column());
    if (!nextItem.isValid()) {
        QModelIndex folder = index(current.parent().row() - 1, 0, QModelIndex());
        if (!folder.isValid()) {
            return current;
        }
        while (folder.isValid() && rowCount(folder) == 0) {
            folder = folder.sibling(folder.row() - 1, folder.column());
        }
        if (folder.isValid() && rowCount(folder) > 0) {
            return index(rowCount(folder) - 1, current.column(), folder);
        }
        nextItem = current;
    }
    return nextItem;
}

QModelIndex AssetFilter::firstVisibleItem(const QModelIndex &current)
{
    if (current.isValid() && isVisible(mapToSource(current))) {
        return current;
    }
    QModelIndex folder = index(0, 0, QModelIndex());
    if (!folder.isValid()) {
        return current;
    }
    while (folder.isValid() && rowCount(folder) == 0) {
        folder = index(folder.row() + 1, 0, QModelIndex());
    }
    if (rowCount(folder) > 0) {
        return index(0, 0, folder);
    }
    return current;
}

QModelIndex AssetFilter::getCategory(int catRow)
{
    QModelIndex cat = index(catRow, 0, QModelIndex());
    return cat;
}

QVariantList AssetFilter::getCategories()
{
    QVariantList list;
    for (int i = 0; i < sourceModel()->rowCount(); i++) {
        QModelIndex cat = getCategory(i);
        if (cat.isValid()) {
            list << cat;
        }
    }
    return list;
}

QModelIndex AssetFilter::getModelIndex(QModelIndex current)
{
    QModelIndex sourceIndex = mapToSource(current);
    return sourceIndex; // this returns an integer
}

QModelIndex AssetFilter::getProxyIndex(QModelIndex current)
{
    QModelIndex sourceIndex = mapFromSource(current);
    return sourceIndex; // this returns an integer
}
