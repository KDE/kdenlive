/*
    SPDX-FileCopyrightText: 2026 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "dopefilter.hpp"

#include "abstractmodel/abstracttreemodel.hpp"
#include "abstractmodel/treeitem.hpp"
#include <KLocalizedString>

DopeFilter::DopeFilter(QObject *parent)
    : QSortFilterProxyModel(parent)

{
    setFilterRole(Qt::DisplayRole);
    setSortRole(Qt::DisplayRole);
    setDynamicSortFilter(false);
}

void DopeFilter::setFilterName(const QString &pattern)
{
    beginFilterChange();
    m_nameFilter = pattern;
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
    if (rowCount() > 1) {
        sort(0);
    }
    Q_EMIT expandAll();
}

bool DopeFilter::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    QString leftData = sourceModel()->data(left).toString();
    QString rightData = sourceModel()->data(right).toString();
    return QString::localeAwareCompare(leftData, rightData) < 0;
}

bool DopeFilter::filterName(const std::shared_ptr<TreeItem> &item) const
{
    if (m_nameFilter.isEmpty()) {
        return true;
    }
    QString itemText = i18n(item->dataColumn(0).toString().toUtf8().constData());
    itemText = normalizeText(itemText);
    QString patt = normalizeText(m_nameFilter);
    return itemText.contains(patt, Qt::CaseInsensitive);
}

QString DopeFilter::normalizeText(const QString &text)
{
    // NormalizationForm_D decomposes letters with diacritics, and then the
    // diacritics are removed by checking isLetterOrNumber().
    const QString normalized = text.normalized(QString::NormalizationForm_D);
    QString newString;
    std::copy_if(normalized.begin(), normalized.end(), std::back_inserter(newString), [](QChar c) { return c.isLetterOrNumber() || c.isSpace(); });
    return newString;
}

bool DopeFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    QModelIndex row = sourceModel()->index(sourceRow, 0, sourceParent);
    auto *model = static_cast<AbstractTreeModel *>(sourceModel());
    std::shared_ptr<TreeItem> item = model->getItemById(int(row.internalId()));
    if (sourceModel()->rowCount(row) == 0) {
        // This is a parameter
    } else {
        // This is a recap, only show if it has children.
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

bool DopeFilter::isVisible(const QModelIndex &sourceIndex)
{
    auto parent = sourceModel()->parent(sourceIndex);
    if (sourceIndex.row() < 0) {
        return false;
    }
    return filterAcceptsRow(sourceIndex.row(), parent);
}

bool DopeFilter::applyAll(std::shared_ptr<TreeItem> item) const
{
    return filterName(item);
}

QModelIndex DopeFilter::getNextChild(const QModelIndex &current)
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

QModelIndex DopeFilter::getPreviousChild(const QModelIndex &current)
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

QModelIndex DopeFilter::firstVisibleItem(const QModelIndex &current)
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

QModelIndex DopeFilter::getModelIndex(const QModelIndex &current)
{
    QModelIndex sourceIndex = mapToSource(current);
    return sourceIndex; // this returns an integer
}

QModelIndex DopeFilter::getProxyIndex(const QModelIndex &current)
{
    QModelIndex sourceIndex = mapFromSource(current);
    return sourceIndex; // this returns an integer
}
