/*
    SPDX-FileCopyrightText: 2017 Nicolas Carion
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#ifndef TRANSITIONLISTWIDGET_H
#define TRANSITIONLISTWIDGET_H

#include "assets/assetlist/view/assetlistwidget.hpp"
#include "kdenlivesettings.h"

class TransitionListWidgetProxy;

/** @class TransitionListWidget
    @brief This class is a widget that display the list of available effects
 */
class TransitionListWidget : public AssetListWidget
{
    Q_OBJECT

public:
    TransitionListWidget(QWidget *parent = Q_NULLPTR);
    ~TransitionListWidget() override;
    void setFilterType(const QString &type);
    bool isAudio(const QString &assetId) const override;
    /** @brief Return mime type used for drag and drop. It will be kdenlive/composition
     or kdenlive/transition*/
    QString getMimeType(const QString &assetId) const override;
    void updateFavorite(const QModelIndex &index);
    void downloadNewLumas();
    void reloadCustomEffectIx(const QModelIndex &path) override;
    void editCustomAsset(const QModelIndex &index) override;
private:
    TransitionListWidgetProxy *m_proxy;

signals:
    void reloadFavorites();
};

// see https://bugreports.qt.io/browse/QTBUG-57714, don't expose a QWidget as a context property
class TransitionListWidgetProxy : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool showDescription READ showDescription WRITE setShowDescription NOTIFY showDescriptionChanged)
public:
    TransitionListWidgetProxy(TransitionListWidget *parent)
        : QObject(parent)
        , q(parent)
    {
    }
    Q_INVOKABLE QString getName(const QModelIndex &index) const { return q->getName(index); }
    Q_INVOKABLE bool isFavorite(const QModelIndex &index) const { return q->isFavorite(index); }
    Q_INVOKABLE void reloadCustomEffectIx(const QModelIndex &index) const { q->reloadCustomEffectIx(index); }
    Q_INVOKABLE void setFavorite(const QModelIndex &index, bool favorite) const
    {
        q->setFavorite(index, favorite, false);
        q->updateFavorite(index);
    }
    Q_INVOKABLE void setFilterType(const QString &type) { q->setFilterType(type); }
    Q_INVOKABLE QString getDescription(const QModelIndex &index) const { return q->getDescription(false, index); }
    Q_INVOKABLE QVariantMap getMimeData(const QString &assetId) const { return q->getMimeData(assetId); }

    Q_INVOKABLE void activate(const QModelIndex &ix) { q->activate(ix); }

    Q_INVOKABLE void setFilterName(const QString &pattern) { q->setFilterName(pattern); }
    Q_INVOKABLE QString getMimeType(const QString &assetId) const { return q->getMimeType(assetId); }
    Q_INVOKABLE void downloadNewLumas() { q->downloadNewLumas(); }
    bool showDescription() const { return KdenliveSettings::showeffectinfo(); }

    void setShowDescription(bool show)
    {
        KdenliveSettings::setShoweffectinfo(show);
        emit showDescriptionChanged();
    }
signals:
    void showDescriptionChanged();

private:
    TransitionListWidget *q; // NOLINT
};

#endif
