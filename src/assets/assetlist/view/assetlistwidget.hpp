/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "effects/effectsrepository.hpp"
#include <QTreeView>
#include <QWidget>
#include <memory>

class AssetIconProvider;
class AssetFilter;
class AssetTreeModel;
class QToolBar;
class QVBoxLayout;
class QMenu;
class QTextDocument;
class QLineEdit;

/** @class AssetListWidget
    @brief This class is a generic widget that display the list of available assets
 */
class AssetListWidget : public QWidget
{
    Q_OBJECT
    /** @brief Should the descriptive info box be displayed
     */
    Q_PROPERTY(bool showDescription READ showDescription WRITE setShowDescription NOTIFY showDescriptionChanged)

public:
    AssetListWidget(bool isEffect, QWidget *parent = Q_NULLPTR);
    ~AssetListWidget() override;

    virtual bool isEffect() const = 0;

    /** @brief Returns the name of the asset given its model index */
    QString getName(const QModelIndex &index) const;

    /** @brief Returns true if this effect belongs to favorites */
    bool isFavorite(const QModelIndex &index) const;

    /** @brief Sets whether this effect belongs to favorites */
    void setFavorite(const QModelIndex &index, bool favorite = true);

    /** @brief Delete a custom effect */
    void deleteCustomEffect(const QModelIndex &index);
    virtual void reloadCustomEffectIx(const QModelIndex &index) = 0;
    virtual void reloadTemplates() = 0;
    virtual void editCustomAsset(const QModelIndex &index) = 0;
    /** @brief Returns the description of the asset given its model index */
    QString getDescription(const QModelIndex &index) const;

    /** @brief Sets the pattern against which the assets' names are filtered */
    void setFilterName(const QString &pattern);

    virtual void setFilterType(const QString &type) = 0;

    /** @brief Return mime type used for drag and drop. It can be kdenlive/effect,
     *  kdenlive/composition or kdenlive/transition
     */
    virtual QString getMimeType(const QString &assetId) const = 0;
    virtual bool isAudio(const QString &assetId) const = 0;

    QVariantMap getMimeData(const QString &assetId) const;

    void activate(const QModelIndex &ix);
    virtual void exportCustomEffect(const QModelIndex &index) = 0;

    /** @brief Should the descriptive info box be displayed
     */
    bool showDescription() const;
    void setShowDescription(bool show);

private:
    QToolBar *m_toolbar;
    QVBoxLayout *m_lay;
    QTextDocument *m_infoDocument;

protected:
    bool m_isEffect;
    std::shared_ptr<AssetTreeModel> m_model;
    std::unique_ptr<AssetFilter> m_proxyModel;
    QMenu *m_contextMenu;
    QLineEdit *m_searchLine;
    QTreeView *m_effectsTree;
    bool eventFilter(QObject *obj, QEvent *event) override;
    const QString buildLink(const QString id, AssetListType::AssetType type) const;

private Q_SLOTS:
    void onCustomContextMenu(const QPoint &pos);

public Q_SLOTS:
    void updateAssetInfo(const QModelIndex &current, const QModelIndex &);
    virtual void reloadCustomEffect(const QString &path) = 0;

Q_SIGNALS:
    void activateAsset(const QVariantMap data);
    void showDescriptionChanged();
    void reloadFavorites();
};
