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

#ifndef TRANSITIONLISTWIDGET_H
#define TRANSITIONLISTWIDGET_H

#include "assets/assetlist/view/assetlistwidget.hpp"
#include "kdenlivesettings.h"

class TransitionListWidgetProxy;

/* @brief This class is a widget that display the list of available effects
 */

class TransitionListWidget : public AssetListWidget
{
    Q_OBJECT

public:
    TransitionListWidget(QWidget *parent = Q_NULLPTR);
    ~TransitionListWidget() override;
    void setFilterType(const QString &type);
    /*@brief Return mime type used for drag and drop. It will be kdenlive/composition
     or kdenlive/transition*/
    QString getMimeType(const QString &assetId) const override;
    void updateFavorite(const QModelIndex &index);
    void downloadNewLumas();

private:
    TransitionListWidgetProxy *m_proxy;
    int getNewStuff(const QString &configFile);

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
