/*
SPDX-FileCopyrightText: 2014 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QCollator>
#include <QSortFilterProxyModel>

class QItemSelectionModel;

/**
 * @class ProjectSortProxyModel
 * @brief Acts as an filtering proxy for the Bin Views, used when triggering the lineedit filter.
 */
class ProjectSortProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    explicit ProjectSortProxyModel(QObject *parent = nullptr);
    QItemSelectionModel *selectionModel();

public slots:
    /** @brief Set search string that will filter the view */
    void slotSetSearchString(const QString &str);
    /** @brief Set search tag that will filter the view */
    void slotSetFilters(const QStringList &tagFilters, int rateFilters, int typeFilters, bool unusedFilter);
    /** @brief Reset search filters */
    void slotClearSearchFilters();
    /** @brief Relay datachanged signal from view's model  */
    void slotDataChanged(const QModelIndex &ix1, const QModelIndex &ix2, const QVector<int> &roles);
    /** @brief Select all items in model */
    void selectAll(const QModelIndex &rootIndex = QModelIndex());

private slots:
    /** @brief Called when a row change is detected by selection model */
    void onCurrentRowChanged(const QItemSelection &current, const QItemSelection &previous);

protected:
    /** @brief Decide which items should be displayed depending on the search string  */
    // cppcheck-suppress unusedFunction
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    /** @brief Reimplemented to show folders first  */
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    bool filterAcceptsRowItself(int source_row, const QModelIndex &source_parent) const;
    bool hasAcceptedChildren(int source_row, const QModelIndex &source_parent) const;

private:
    QItemSelectionModel *m_selection;
    QString m_searchString;
    QStringList m_searchTag;
    int m_searchType{0};
    int m_searchRating{0};
    bool m_unusedFilter{false};
    QCollator m_collator;

signals:
    /** @brief Emitted when the row changes, used to prepare action for selected item  */
    void selectModel(const QModelIndex &);
    /** @brief Set item rating */
    void updateRating(const QModelIndex &index, uint rating);
};
