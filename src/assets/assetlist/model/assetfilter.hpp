/***************************************************************************
 *   Copyright (C) 2017 by Nicolas Carion                                  *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifndef ASSETFILTER_H
#define ASSETFILTER_H

#include <QSortFilterProxyModel>
#include <memory>

/* @brief This class is used as a proxy model to filter an asset list based on given criterion (name, ...)
 */
class TreeItem;
class AssetFilter : public QSortFilterProxyModel
{
    Q_OBJECT

public:
    AssetFilter(QObject *parent = nullptr);

    /* @brief Manage the name filter
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
    Q_INVOKABLE QModelIndex getModelIndex(QModelIndex current);
    Q_INVOKABLE QModelIndex getProxyIndex(QModelIndex current);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;
    bool filterName(const std::shared_ptr<TreeItem> &item) const;
    /* @brief Apply all filter and returns true if the object should be kept after filtering */
    virtual bool applyAll(std::shared_ptr<TreeItem> item) const;

    bool m_name_enabled{false};
    QString m_name_value;
};
#endif
