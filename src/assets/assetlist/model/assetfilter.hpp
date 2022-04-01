/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include <QSortFilterProxyModel>
#include <memory>

/** @brief This class is used as a proxy model to filter an asset list based on given criterion (name, ...)
 */
class TreeItem;
class AssetFilter : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    AssetFilter(QObject *parent = nullptr);

    /** @brief Manage the name filter
       @param enabled whether to enable this filter
       @param pattern to match against effects' names
    */
    void setFilterName(bool enabled, const QString &pattern);

    /** @brief Returns true if the ModelIndex in the source model is visible after filtering
     */
    bool isVisible(const QModelIndex &sourceIndex);

    /** @brief If we are in favorite view, invalidate filter to refresh. Call this after a favorite has changed
     */
    virtual void reloadFilterOnFavorite() = 0;

    QVariantList getCategories();
    Q_INVOKABLE QModelIndex getNextChild(const QModelIndex &current);
    Q_INVOKABLE QModelIndex getPreviousChild(const QModelIndex &current);
    Q_INVOKABLE QModelIndex firstVisibleItem(const QModelIndex &current);
    Q_INVOKABLE QModelIndex getCategory(int catRow);
    Q_INVOKABLE QModelIndex getModelIndex(const QModelIndex &current);
    Q_INVOKABLE QModelIndex getProxyIndex(const QModelIndex &current);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    bool filterName(const std::shared_ptr<TreeItem> &item) const;
    /** @brief Apply all filter and returns true if the object should be kept after filtering */
    virtual bool applyAll(std::shared_ptr<TreeItem> item) const;

    bool m_name_enabled{false};
    QString m_name_value;
};
