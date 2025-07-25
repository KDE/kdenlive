/*
SPDX-FileCopyrightText: 2025 Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QCollator>
#include <QSortFilterProxyModel>

/**
 * @class TreeProxyModel
 * @brief Acts as an filtering proxy for the render model.
 */
class TreeProxyModel : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    enum UsageFilter { All, Used, Unused };

    explicit TreeProxyModel(QObject *parent = nullptr);

public Q_SLOTS:
    /** @brief Set search string that will filter the view */
    void slotSetSearchString(const QString &str);

protected:
    /** @brief Decide which items should be displayed depending on the search string  */
    // cppcheck-suppress unusedFunction
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool filterAcceptsRowItself(int source_row, const QModelIndex &source_parent) const;
    bool hasAcceptedChildren(int source_row, const QModelIndex &source_parent) const;

private:
    QString m_searchString;
    UsageFilter m_usageFilter{UsageFilter::All};
    QCollator m_collator;

Q_SIGNALS:
    /** @brief Emitted when the row changes, used to prepare action for selected item  */
    void selectModel(const QModelIndex &);
    /** @brief Set item rating */
    void updateRating(const QModelIndex &index, uint rating);
};
