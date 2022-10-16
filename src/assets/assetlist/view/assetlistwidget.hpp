/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

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
    Q_PROPERTY(bool showDescription READ showDescription WRITE setShowDescription NOTIFY showDescriptionChanged)

public:
    AssetListWidget(QWidget *parent = Q_NULLPTR);
    ~AssetListWidget() override;

    virtual bool isEffect() const = 0;

    /** @brief Returns the name of the asset given its model index */
    Q_INVOKABLE QString getName(const QModelIndex &index) const;

    /** @brief Returns true if this effect belongs to favorites */
    Q_INVOKABLE bool isFavorite(const QModelIndex &index) const;

    /** @brief Sets whether this effect belongs to favorites */
    Q_INVOKABLE void setFavorite(const QModelIndex &index, bool favorite = true);

    /** @brief Delete a custom effect */
    Q_INVOKABLE void deleteCustomEffect(const QModelIndex &index);
    Q_INVOKABLE virtual void reloadCustomEffectIx(const QModelIndex &index) = 0;
    Q_INVOKABLE virtual void editCustomAsset(const QModelIndex &index) = 0;
    /** @brief Returns the description of the asset given its model index */
    Q_INVOKABLE QString getDescription(const QModelIndex &index) const;

    /** @brief Sets the pattern against which the assets' names are filtered */
    Q_INVOKABLE void setFilterName(const QString &pattern);

    Q_INVOKABLE virtual void setFilterType(const QString &type) = 0;

    /** @brief Return mime type used for drag and drop. It can be kdenlive/effect,
     *  kdenlive/composition or kdenlive/transition
     */
    Q_INVOKABLE virtual QString getMimeType(const QString &assetId) const = 0;
    virtual bool isAudio(const QString &assetId) const = 0;

    Q_INVOKABLE QVariantMap getMimeData(const QString &assetId) const;

    Q_INVOKABLE void activate(const QModelIndex &ix);

    /** @brief Should the descriptive info box be displayed
     */
    bool showDescription() const;
    void setShowDescription(bool show);

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
    void showDescriptionChanged();
    void reloadFavorites();
};
