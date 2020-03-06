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

#ifndef EFFECTLISTWIDGET_H
#define EFFECTLISTWIDGET_H

#include "assets/assetlist/view/assetlistwidget.hpp"
#include "kdenlivesettings.h"

/* @brief This class is a widget that display the list of available effects
 */

class EffectFilter;
class EffectTreeModel;
class EffectListWidgetProxy;
class KActionCategory;
class QMenu;

class EffectListWidget : public AssetListWidget
{
    Q_OBJECT

public:
    EffectListWidget(QWidget *parent = Q_NULLPTR);
    ~EffectListWidget() override;
    void setFilterType(const QString &type);

    /*@brief Return mime type used for drag and drop. It will be kdenlive/effect*/
    QString getMimeType(const QString &assetId) const override;
    void updateFavorite(const QModelIndex &index);
    void reloadEffectMenu(QMenu *effectsMenu, KActionCategory *effectActions);

public slots:
    void reloadCustomEffect(const QString &path);

private:
    EffectListWidgetProxy *m_proxy;

signals:
    void reloadFavorites();
};

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
    Q_INVOKABLE void setFavorite(const QModelIndex &index, bool favorite) const
    {
        q->setFavorite(index, favorite, true);
        q->updateFavorite(index);
    }
    Q_INVOKABLE void deleteCustomEffect(const QModelIndex &index) { q->deleteCustomEffect(index); }
    Q_INVOKABLE QString getDescription(const QModelIndex &index) const { return q->getDescription(true, index); }
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

#endif
