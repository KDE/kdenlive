/*
SPDX-FileCopyrightText: 2025 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "treeproxymodel.h"

#include <QItemSelectionModel>

TreeProxyModel::TreeProxyModel(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    m_collator.setLocale(QLocale()); // Locale used for sorting → OK
    m_collator.setCaseSensitivity(Qt::CaseInsensitive);
    m_collator.setNumericMode(true);
    setDynamicSortFilter(true);
}

// Responsible for item sorting!
bool TreeProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    if (filterAcceptsRowItself(sourceRow, sourceParent)) {
        return true;
    }
    // accept if any of the children is accepted on it's own merits
    return hasAcceptedChildren(sourceRow, sourceParent);
}

bool TreeProxyModel::filterAcceptsRowItself(int sourceRow, const QModelIndex &sourceParent) const
{
    if (m_searchString.isEmpty()) {
        return true;
    }
    bool result = false;
    for (int i = 0; i < 3; i++) {
        QModelIndex index0 = sourceModel()->index(sourceRow, i, sourceParent);
        if (!index0.isValid()) {
            return false;
        }
        auto data = sourceModel()->data(index0);
        if (data.toString().contains(m_searchString, Qt::CaseInsensitive)) {
            result = true;
            break;
        }
    }
    return result;
}

bool TreeProxyModel::hasAcceptedChildren(int sourceRow, const QModelIndex &source_parent) const
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

void TreeProxyModel::slotSetSearchString(const QString &str)
{
    m_searchString = str;
    invalidateFilter();
}
