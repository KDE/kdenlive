/*
    SPDX-FileCopyrightText: 2026 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QSortFilterProxyModel>
#include <QtQmlIntegration/qqmlintegration.h>
#include <memory>

/** @brief This class is used as a proxy model to filter the dopesheet view
 */
class TreeItem;
class DopeFilter : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("DopeFilter is owned by Core; obtained via setInitialProperties()")

public:
    DopeFilter(QObject *parent = nullptr);

    /** @brief Returns true if the ModelIndex in the source model is visible after filtering
     */
    bool isVisible(const QModelIndex &sourceIndex);

    Q_INVOKABLE QModelIndex getNextChild(const QModelIndex &current);
    Q_INVOKABLE QModelIndex getPreviousChild(const QModelIndex &current);
    Q_INVOKABLE QModelIndex firstVisibleItem(const QModelIndex &current);
    Q_INVOKABLE QModelIndex getModelIndex(const QModelIndex &current);
    Q_INVOKABLE QModelIndex getProxyIndex(const QModelIndex &current);

public Q_SLOTS:
    /** @brief Manage the name filter
       @param enabled whether to enable this filter
       @param pattern to match against effects' names
    */
    void setFilterName(const QString &pattern);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    /** @brief Given a TreeItem, check if the ID and Name (first and second)
     * columns contain the filtered name as a substring.
     * @return true if either ID or Name contains the filtered name as a
     * substring, false otherwise
     */
    bool filterName(const std::shared_ptr<TreeItem> &item) const;
    /** @brief Returns a copy of the input string with any characters that are
     * not letters, numbers, or spaces removed. */
    static QString normalizeText(const QString &text);
    /** @brief Apply all filter and returns true if the object should be kept after filtering */
    virtual bool applyAll(std::shared_ptr<TreeItem> item) const;
    QString m_nameFilter;

Q_SIGNALS:
    void expandAll();
};
