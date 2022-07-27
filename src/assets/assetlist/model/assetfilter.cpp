/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "assetfilter.hpp"

#include "abstractmodel/abstracttreemodel.hpp"
#include "abstractmodel/treeitem.hpp"
#include "assettreemodel.hpp"
#include <klocalizedstring.h>
#include <utility>

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

bool AssetFilter::lessThan(const QModelIndex &left, const QModelIndex &right) const
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
    const QString itemId = normalizeText(item->dataColumn(AssetTreeModel::IdCol).toString());
    QString itemText = i18n(item->dataColumn(AssetTreeModel::NameCol).toString().toUtf8().constData());
    itemText = normalizeText(itemText);
    QString patt = normalizeText(m_name_value);

    return itemText.contains(patt, Qt::CaseInsensitive) || itemId.contains(patt, Qt::CaseInsensitive);
}

QString AssetFilter::normalizeText(const QString &text)
{
    // NormalizationForm_D decomposes letters with diacritics, and then the
    // diacritics are removed by checking isLetterOrNumber().
    const QString normalized = text.normalized(QString::NormalizationForm_D);
    QString newString;
    std::copy_if(normalized.begin(), normalized.end(), std::back_inserter(newString),
        [](QChar c){
            return c.isLetterOrNumber() || c.isSpace();
        });
    return newString;
}

bool AssetFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex row = sourceModel()->index(sourceRow, 0, sourceParent);
    auto *model = static_cast<AbstractTreeModel *>(sourceModel());
    std::shared_ptr<TreeItem> item = model->getItemById(int(row.internalId()));

    if (item->dataColumn(AssetTreeModel::IdCol) == QStringLiteral("root")) {
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

QModelIndex AssetFilter::getModelIndex(const QModelIndex &current)
{
    QModelIndex sourceIndex = mapToSource(current);
    return sourceIndex; // this returns an integer
}

QModelIndex AssetFilter::getProxyIndex(const QModelIndex &current)
{
    QModelIndex sourceIndex = mapFromSource(current);
    return sourceIndex; // this returns an integer
}
