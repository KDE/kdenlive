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

#ifndef ASSETLISTWIDGET_H
#define ASSETLISTWIDGET_H

#include "effects/effectsrepository.hpp"
#include <QQuickWidget>
#include <memory>

/* @brief This class is a generic widget that display the list of available assets
 */

class AssetIconProvider;
class AssetFilter;
class AssetTreeModel;

class AssetListWidget : public QQuickWidget
{
    Q_OBJECT
    /* @brief Should the descriptive info box be displayed
     */

public:
    AssetListWidget(QWidget *parent = Q_NULLPTR);
    ~AssetListWidget() override;

    /* @brief Returns the name of the asset given its model index */
    QString getName(const QModelIndex &index) const;

    /* @brief Returns true if this effect belongs to favorites */
    bool isFavorite(const QModelIndex &index) const;

    /* @brief Sets whether this effect belongs to favorites */
    void setFavorite(const QModelIndex &index, bool favorite = true, bool isEffect = true);

    /* @brief Delete a custom effect */
    void deleteCustomEffect(const QModelIndex &index);

    /* @brief Returns the description of the asset given its model index */
    QString getDescription(bool isEffect, const QModelIndex &index) const;

    /* @brief Sets the pattern against which the assets' names are filtered */
    void setFilterName(const QString &pattern);

    /*@brief Return mime type used for drag and drop. It can be kdenlive/effect,
      kdenlive/composition or kdenlive/transition*/
    virtual QString getMimeType(const QString &assetId) const = 0;

    QVariantMap getMimeData(const QString &assetId) const;

    void activate(const QModelIndex &ix);

    /* @brief Rebuild the view by resetting the source. Is there a better way? */
    void reset();

protected:
    void setup();
    std::shared_ptr<AssetTreeModel> m_model;
    std::unique_ptr<AssetFilter> m_proxyModel;
    // the QmlEngine takes ownership of the image provider
    AssetIconProvider *m_assetIconProvider{nullptr};

signals:
    void activateAsset(const QVariantMap data);
};

#endif
