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
class QTextBrowser;
class QToolBar;
class QVBoxLayout;
class QMenu;
class QTextDocument;
class QLineEdit;
class QListView;
class QStackedWidget;
class QToolButton;

/** @class AssetListWidget
    @brief This class is the widget that display the list of available assets (effects or compositions)
    @author Nicolas Carion
*/
class AssetListWidget : public QWidget
{
    Q_OBJECT
    /** @brief Should the descriptive info box be displayed
     */
    Q_PROPERTY(bool showDescription READ showDescription WRITE setShowDescription NOTIFY showDescriptionChanged)

public:
    AssetListWidget(bool isEffect, QAction *includeList, QAction *tenBit, QWidget *parent = Q_NULLPTR);
    ~AssetListWidget() override;

    /** @brief Returns true if the asset is a favorite */
    bool isFavorite(const QModelIndex &index) const;
    /** @brief Set the asset as favorite or not */
    void setFavorite(const QModelIndex &index, bool favorite);
    /** @brief Delete a custom effect */
    void deleteCustomEffect(const QModelIndex &index);
    /** @brief Returns the name of the asset */
    QString getName(const QModelIndex &index) const;
    /** @brief Returns the description of the asset */
    QString getDescription(const QModelIndex &index) const;
    /** @brief Returns true if we should show the description panel */
    bool showDescription() const;
    /** @brief Set whether we should show the description panel */
    void setShowDescription(bool show);
    /** @brief Returns true if the asset is an audio asset */
    virtual bool isAudio(const QString &assetId) const = 0;
    /** @brief Returns the mime type for this asset */
    virtual QString getMimeType(const QString &assetId) const = 0;
    /** @brief Returns the mime data for this asset */
    QVariantMap getMimeData(const QString &assetId) const;
    /** @brief Returns true if the asset is a custom one */
    static bool isCustomType(AssetListType::AssetType itemType);
    /** @brief Returns true if the asset is an audio one */
    static bool isAudioType(AssetListType::AssetType type);
    /** @brief Build a link to the online documentation for this asset */
    static const QString buildLink(const QString &id, AssetListType::AssetType type);
    /** @brief Returns true if we are displaying effects */
    virtual bool isEffect() const { return m_isEffect; }
    /** @brief Reload a custom effect */
    virtual void reloadCustomEffect(const QString &path) = 0;
    /** @brief Reload a custom effect */
    virtual void reloadCustomEffectIx(const QModelIndex &index) = 0;
    /** @brief Reload all templates */
    virtual void reloadTemplates() = 0;
    /** @brief Edit a custom asset */
    virtual void editCustomAsset(const QModelIndex &index) = 0;
    /** @brief Export a custom effect */
    virtual void exportCustomEffect(const QModelIndex &index) = 0;
    /** @brief Set the filter type */
    virtual void setFilterType(const QString &type) = 0;

    /** @brief Toggle between tree view and icon view */
    void toggleViewMode(bool checked);

    /** @brief Returns true if we are in icon view mode */
    bool isIconView() const;

    void activate(const QModelIndex &ix);
    virtual void switchTenBitFilter() = 0;

    bool infoPanelIsFocused();
    void processCopy();

private:
    QTextBrowser *m_textEdit;
    QVBoxLayout *m_lay;
    QTextDocument *m_infoDocument;

protected:
    QTreeView *m_effectsTree;
    QListView *m_effectsIcon;
    QToolBar *m_toolbar;
    QStackedWidget *m_effectsView;
    QMenu *m_contextMenu;
    QLineEdit *m_searchLine;
    std::shared_ptr<AssetTreeModel> m_model;
    std::unique_ptr<AssetFilter> m_proxyModel;
    bool m_isEffect;
    AssetIconProvider *m_assetIconProvider;
    QToolButton *m_filterButton;
    bool eventFilter(QObject *obj, QEvent *event) override;

private Q_SLOTS:
    /** @brief Display the context menu */
    void onCustomContextMenu(const QPoint &pos);

public Q_SLOTS:
    /** @brief Set the filter name */
    void setFilterName(const QString &pattern);
    /** @brief Update the info panel */
    void updateAssetInfo(const QModelIndex &current, const QModelIndex &previous);
    void setItemFavorite();

Q_SIGNALS:
    void activateAsset(const QVariantMap &);
    void reloadFavorites();
    void showDescriptionChanged();
};
