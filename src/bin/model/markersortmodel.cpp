/*
SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "markersortmodel.h"
#include "markerlistmodel.hpp"

MarkerSortModel::MarkerSortModel(QObject *parent)
    : QSortFilterProxyModel(parent)
    , m_sortColumn(1)
    , m_sortOrder(0)
{
    setDynamicSortFilter(true);
}

// Responsible for item sorting!
bool MarkerSortModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (filterAcceptsRowItself(sourceRow, sourceParent)) {
        return true;
    }
    return false;
}

bool MarkerSortModel::filterAcceptsRowItself(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_filterList.isEmpty()) {
        return filterString(sourceRow, sourceParent);
    }
    QModelIndex row = sourceModel()->index(sourceRow, 0, sourceParent);
    if (m_filterList.contains(sourceModel()->data(row, MarkerListModel::TypeRole).toInt())) {
        return filterString(sourceRow, sourceParent);
    } else {
        m_ignoredPositions << sourceModel()->data(row, MarkerListModel::FrameRole).toInt();
    }
    return false;
}

bool MarkerSortModel::filterString(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_searchString.isEmpty()) {
        return true;
    }
    QModelIndex index0 = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!index0.isValid()) {
        return false;
    }
    auto data = sourceModel()->data(index0);
    if (data.toString().contains(m_searchString, Qt::CaseInsensitive)) {
        return true;
    }
    return false;
}

void MarkerSortModel::slotSetFilters(const QList<int> filters)
{
    m_filterList = filters;
    m_ignoredPositions.clear();
    invalidateFilter();
}

void MarkerSortModel::slotClearSearchFilters()
{
    m_filterList.clear();
    m_ignoredPositions.clear();
    invalidateFilter();
}

void MarkerSortModel::slotSetFilterString(const QString &filter)
{
    m_searchString = filter;
    invalidateFilter();
}

std::vector<int> MarkerSortModel::getIgnoredSnapPoints() const
{
    std::vector<int> markers(m_ignoredPositions.cbegin(), m_ignoredPositions.cend());
    return markers;
}

void MarkerSortModel::slotSetSortColumn(int column)
{
    m_sortColumn = column;
    invalidate();
}

void MarkerSortModel::slotSetSortOrder(bool descending)
{
    m_sortOrder = descending ? Qt::DescendingOrder : Qt::AscendingOrder;
    invalidate();
}

bool MarkerSortModel::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    switch (m_sortColumn) {
    case 0: {
        // Sort by category
        int leftCategory = sourceModel()->data(left, MarkerListModel::TypeRole).toInt();
        int rightCategory = sourceModel()->data(right, MarkerListModel::TypeRole).toInt();
        if (leftCategory == rightCategory) {
            // sort by time
            double leftTime = sourceModel()->data(left, MarkerListModel::PosRole).toDouble();
            double rightTime = sourceModel()->data(right, MarkerListModel::PosRole).toDouble();
            if (m_sortOrder == Qt::AscendingOrder) {
                return leftTime < rightTime;
            }
            return leftTime > rightTime;
        }
        if (m_sortOrder == Qt::AscendingOrder) {
            return leftCategory < rightCategory;
        }
        return leftCategory > rightCategory;
    }
    case 2: {
        // Sort by comment
        const QString leftComment = sourceModel()->data(left, MarkerListModel::CommentRole).toString();
        const QString rightComment = sourceModel()->data(right, MarkerListModel::CommentRole).toString();
        if (leftComment == rightComment) {
            // sort by time
            double leftTime = sourceModel()->data(left, MarkerListModel::PosRole).toDouble();
            double rightTime = sourceModel()->data(right, MarkerListModel::PosRole).toDouble();
            if (m_sortOrder == Qt::AscendingOrder) {
                return leftTime < rightTime;
            }
            return leftTime > rightTime;
        }
        if (m_sortOrder == Qt::AscendingOrder) {
            return QString::localeAwareCompare(leftComment, rightComment) < 0;
        }
        return QString::localeAwareCompare(leftComment, rightComment) > 0;
    }
    case 1:
    default: {
        double leftTime = sourceModel()->data(left, MarkerListModel::PosRole).toDouble();
        double rightTime = sourceModel()->data(right, MarkerListModel::PosRole).toDouble();
        if (m_sortOrder == Qt::AscendingOrder) {
            return leftTime < rightTime;
        }
        return leftTime > rightTime;
    }
    }
}
