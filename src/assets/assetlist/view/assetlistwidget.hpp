/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#ifndef ASSETLISTWIDGET_H
#define ASSETLISTWIDGET_H

#include "effects/effectsrepository.hpp"
#include <QQuickWidget>
#include <memory>


class AssetIconProvider;
class AssetFilter;
class AssetTreeModel;

/** @class AssetListWidget
    @brief This class is a generic widget that display the list of available assets
 */
class AssetListWidget : public QQuickWidget
{
    Q_OBJECT
    /** @brief Should the descriptive info box be displayed
     */

public:
    AssetListWidget(QWidget *parent = Q_NULLPTR);
    ~AssetListWidget() override;

    /** @brief Returns the name of the asset given its model index */
    QString getName(const QModelIndex &index) const;

    /** @brief Returns true if this effect belongs to favorites */
    bool isFavorite(const QModelIndex &index) const;

    /** @brief Sets whether this effect belongs to favorites */
    void setFavorite(const QModelIndex &index, bool favorite = true, bool isEffect = true);

    /** @brief Delete a custom effect */
    void deleteCustomEffect(const QModelIndex &index);
    virtual void reloadCustomEffectIx(const QModelIndex &index) = 0;
    virtual void editCustomAsset(const QModelIndex &index) = 0;
    /** @brief Returns the description of the asset given its model index */
    QString getDescription(bool isEffect, const QModelIndex &index) const;

    /** @brief Sets the pattern against which the assets' names are filtered */
    void setFilterName(const QString &pattern);

    /** @brief Return mime type used for drag and drop. It can be kdenlive/effect,
      kdenlive/composition or kdenlive/transition*/
    virtual QString getMimeType(const QString &assetId) const = 0;

    QVariantMap getMimeData(const QString &assetId) const;

    void activate(const QModelIndex &ix);

    /** @brief Rebuild the view by resetting the source. Is there a better way? */
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
