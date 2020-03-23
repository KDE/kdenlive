/***************************************************************************
 *   Copyright (C) 2016 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License as        *
 *   published by the Free Software Foundation; either version 2 of        *
 *   the License or (at your option) version 3 or any later version        *
 *   accepted by the membership of KDE e.V. (or its successor approved     *
 *   by the membership of KDE e.V.), which shall act as a proxy            *
 *   defined in Section 14 of version 3 of the license.                    *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

/*!
 * @class QmlManager
 * @brief Manages all Qml monitor overlays
 * @author Jean-Baptiste Mardelle
 */

#ifndef QMLMANAGER_H
#define QMLMANAGER_H

#include "definitions.h"

class QQuickView;

class QmlManager : public QObject
{
    Q_OBJECT

public:
    explicit QmlManager(QQuickView *view);

    /** @brief return current active scene type */
    MonitorSceneType sceneType() const;
    /** @brief Set a property on the root item */
    void setProperty(const QString &name, const QVariant &value);
    /** @brief Load a monitor scene */
    void setScene(Kdenlive::MonitorId id, MonitorSceneType type, QSize profile, double profileStretch, QRect displayRect, double zoom, int duration);

private:
    QQuickView *m_view;
    MonitorSceneType m_sceneType;

private slots:
    void effectRectChanged();
    void effectPolygonChanged();
    void effectRotoChanged();

signals:
    void effectChanged(const QRect &);
    void effectPointsChanged(const QVariantList &);
    void activateTrack(int);
};

#endif
