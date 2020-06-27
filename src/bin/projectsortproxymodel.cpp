/*
Copyright (C) 2014  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "projectsortproxymodel.h"
#include "abstractprojectitem.h"

#include <QItemSelectionModel>

ProjectSortProxyModel::ProjectSortProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_searchType(0)
    , m_searchRating(0)
{
    m_collator.setLocale(QLocale()); // Locale used for sorting → OK
    m_collator.setCaseSensitivity(Qt::CaseInsensitive);
    m_collator.setNumericMode(true);
    m_selection = new QItemSelectionModel(this);
    connect(m_selection, &QItemSelectionModel::selectionChanged, this, &ProjectSortProxyModel::onCurrentRowChanged);
    setDynamicSortFilter(true);
}

// Responsible for item sorting!
bool ProjectSortProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (filterAcceptsRowItself(sourceRow, sourceParent)) {
        return true;
    }
    // accept if any of the children is accepted on it's own merits
    return hasAcceptedChildren(sourceRow, sourceParent);
}

bool ProjectSortProxyModel::filterAcceptsRowItself(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_searchRating > 0) {
        // Column 7 contains the rating
        QModelIndex indexTag = sourceModel()->index(sourceRow, 7, sourceParent);
        if (sourceModel()->data(indexTag).toInt() != m_searchRating) {
            return false;
        }
    }
    if (m_searchType > 0) {
        // Column 3 contains the item type (video, image, title, etc)
        QModelIndex indexTag = sourceModel()->index(sourceRow, 3, sourceParent);
        if (sourceModel()->data(indexTag).toInt() != m_searchType) {
            return false;
        }
    }
    if (!m_searchTag.isEmpty()) {
        // Column 4 contains the item tag data
        QModelIndex indexTag = sourceModel()->index(sourceRow, 4, sourceParent);
        auto tagData = sourceModel()->data(indexTag);
        for (const QString &tag : m_searchTag) {
            if (!tagData.toString().contains(tag, Qt::CaseInsensitive)) {
                return false;
            }
        }
    }

    for (int i = 0; i < 3; i++) {
        QModelIndex index0 = sourceModel()->index(sourceRow, i, sourceParent);
        if (!index0.isValid()) {
            return false;
        }
        auto data = sourceModel()->data(index0);
        if (data.toString().contains(m_searchString, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

bool ProjectSortProxyModel::hasAcceptedChildren(int sourceRow, const QModelIndex &source_parent) const
{
    QModelIndex item = sourceModel()->index(sourceRow, 0, source_parent);
    if (!item.isValid()) {
        return false;
    }

    // check if there are children
    int childCount = item.model()->rowCount(item);
    if (childCount == 0) {
        return false;
    }

    for (int i = 0; i < childCount; ++i) {
        if (filterAcceptsRowItself(i, item)) {
            return true;
        }
        // recursive call -> NOTICE that this is depth-first searching, you're probably better off with breadth first search...
        if (hasAcceptedChildren(i, item)) {
            return true;
        }
    }
    return false;
}

bool ProjectSortProxyModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    // Check item type (folder or clip) as defined in projectitemmodel
    int leftType = sourceModel()->data(left, AbstractProjectItem::ItemTypeRole).toInt();
    int rightType = sourceModel()->data(right, AbstractProjectItem::ItemTypeRole).toInt();
    if (leftType == rightType) {
        // Let the normal alphabetical sort happen
        const QVariant leftData = sourceModel()->data(left, Qt::DisplayRole);
        const QVariant rightData = sourceModel()->data(right, Qt::DisplayRole);
        if (leftData.type() == QVariant::DateTime) {
            return leftData.toDateTime() < rightData.toDateTime();
        }
        return m_collator.compare(leftData.toString(), rightData.toString()) < 0;
    }
    if (sortOrder() == Qt::AscendingOrder) {
        return leftType < rightType;
    }
    return leftType > rightType;
}

QItemSelectionModel *ProjectSortProxyModel::selectionModel()
{
    return m_selection;
}

void ProjectSortProxyModel::slotSetSearchString(const QString &str)
{
    m_searchString = str;
    invalidateFilter();
}

void ProjectSortProxyModel::slotSetFilters(const QStringList tagFilters, const int rateFilters, const int typeFilters)
{
    m_searchType = typeFilters;
    m_searchRating = rateFilters;
    m_searchTag = tagFilters;
    invalidateFilter();
}

void ProjectSortProxyModel::slotClearSearchFilters()
{
    m_searchTag.clear();
    m_searchRating = 0;
    m_searchType = 0;
    invalidateFilter();
}

void ProjectSortProxyModel::onCurrentRowChanged(const QItemSelection &current, const QItemSelection &previous)
{
    Q_UNUSED(previous)
    QModelIndexList indexes = current.indexes();
    if (indexes.isEmpty()) {
        emit selectModel(QModelIndex());
        return;
    }
    for (int ix = 0; ix < indexes.count(); ix++) {
        if (indexes.at(ix).column() == 0 || indexes.at(ix).column() == 7) {
            emit selectModel(indexes.at(ix));
            break;
        }
    }
}

void ProjectSortProxyModel::slotDataChanged(const QModelIndex &ix1, const QModelIndex &ix2, const QVector<int> &roles)
{
    emit dataChanged(ix1, ix2, roles);
}


void ProjectSortProxyModel::selectAll()
{
        QModelIndex topLeft = index(0, 0, QModelIndex());
        QModelIndex bottomRight = index(rowCount(QModelIndex()) - 1,
            columnCount(QModelIndex()) - 1, QModelIndex());
        QItemSelection selection(topLeft, bottomRight);
        m_selection->select(selection, QItemSelectionModel::Select);
}
