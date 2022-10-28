/*
SPDX-FileCopyrightText: 2014 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QCollator>
#include <QSortFilterProxyModel>

/**
 * @class MarkerSortModel
 * @brief Acts as an filtering proxy for the Bin Views, used when triggering the lineedit filter.
 */
class MarkerSortModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit MarkerSortModel(QObject *parent = nullptr);

public slots:
    /** @brief Set search tag that will filter the view */
    void slotSetFilters(const QList<int> filter);
    /** @brief Reset search filters */
    void slotClearSearchFilters();
    std::vector<int> getIgnoredSnapPoints() const;

protected:
    /** @brief Decide which items should be displayed depending on the search string  */
    // cppcheck-suppress unusedFunction
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool filterAcceptsRowItself(int source_row, const QModelIndex &source_parent) const;

private:
    QList<int> m_filterList;
    mutable QList<int> m_ignoredPositions;
};
