/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "assets/assetlist/view/assetlistwidget.hpp"
#include "kdenlivesettings.h"

class EffectFilter;
class EffectTreeModel;
class EffectListWidgetProxy;
class KActionCategory;
class QMenu;

/** @class EffectListWidget
    @brief This class is a widget that display the list of available effects
 */
class EffectListWidget : public AssetListWidget
{
    Q_OBJECT

public:
    EffectListWidget(QWidget *parent = Q_NULLPTR);
    ~EffectListWidget() override;
    void setFilterType(const QString &type);
    bool isAudio(const QString &assetId) const override;
    /** @brief Return mime type used for drag and drop. It will be kdenlive/effect*/
    QString getMimeType(const QString &assetId) const override;
    void updateFavorite(const QModelIndex &index);
    void reloadEffectMenu(QMenu *effectsMenu, KActionCategory *effectActions);
    void reloadCustomEffectIx(const QModelIndex &index) override;
    void editCustomAsset(const QModelIndex &index) override;
    void exportCustomEffect(const QModelIndex &index);
public slots:
    void reloadCustomEffect(const QString &path);

private:
    EffectListWidgetProxy *m_proxy;

signals:
    void reloadFavorites();
};

// TODO we use Qt 5.15 now where this is fixed
// see https://bugreports.qt.io/browse/QTBUG-57714, don't expose a QWidget as a context property
class EffectListWidgetProxy : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool showDescription READ showDescription WRITE setShowDescription NOTIFY showDescriptionChanged)

public:
    EffectListWidgetProxy(EffectListWidget *parent)
        : QObject(parent)
        , q(parent)
    {
    }
    Q_INVOKABLE QString getName(const QModelIndex &index) const { return q->getName(index); }
    Q_INVOKABLE bool isFavorite(const QModelIndex &index) const { return q->isFavorite(index); }
    Q_INVOKABLE void reloadCustomEffectIx(const QModelIndex &index) const { q->reloadCustomEffectIx(index); }
    Q_INVOKABLE void setFavorite(const QModelIndex &index, bool favorite) const
    {
        q->setFavorite(index, favorite, true);
        q->updateFavorite(index);
    }
    Q_INVOKABLE void deleteCustomEffect(const QModelIndex &index) { q->deleteCustomEffect(index); }
    Q_INVOKABLE QString getDescription(const QModelIndex &index) const { return q->getDescription(true, index); }
    Q_INVOKABLE void editCustomEffectInfo(const QModelIndex &index) { q->editCustomAsset(index); }
    Q_INVOKABLE void exportCustomEffect(const QModelIndex &index) { q->exportCustomEffect(index); }
    Q_INVOKABLE QVariantMap getMimeData(const QString &assetId) const { return q->getMimeData(assetId); }

    Q_INVOKABLE void activate(const QModelIndex &ix) { q->activate(ix); }
    Q_INVOKABLE void setFilterType(const QString &type) { q->setFilterType(type); }
    Q_INVOKABLE void setFilterName(const QString &pattern) { q->setFilterName(pattern); }
    Q_INVOKABLE QString getMimeType(const QString &assetId) const { return q->getMimeType(assetId); }

    bool showDescription() const { return KdenliveSettings::showeffectinfo(); }

    void setShowDescription(bool show)
    {
        KdenliveSettings::setShoweffectinfo(show);
        emit showDescriptionChanged();
    }
signals:
    void showDescriptionChanged();

private:
    EffectListWidget *q; // NOLINT
};
