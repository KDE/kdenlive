/*
SPDX-FileCopyrightText: 2022 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "markersortmodel.h"
#include "markerlistmodel.hpp"

MarkerSortModel::MarkerSortModel(QObject *parent)
    : QSortFilterProxyModel(parent)
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
        return true;
    }
    QModelIndex row = sourceModel()->index(sourceRow, 0, sourceParent);
    if (m_filterList.contains(sourceModel()->data(row, MarkerListModel::TypeRole).toInt())) {
        return true;
    } else {
        m_ignoredPositions << sourceModel()->data(row, MarkerListModel::FrameRole).toInt();
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

std::vector<int> MarkerSortModel::getIgnoredSnapPoints() const
{
    std::vector<int> markers(m_ignoredPositions.cbegin(), m_ignoredPositions.cend());
    return markers;
}
