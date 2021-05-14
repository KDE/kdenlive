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

#include "qmlmanager.h"
#include "kdenlivesettings.h"

#include <QFontDatabase>
#include <QQmlContext>
#include <QQuickView>
#include <QQuickItem>

QmlManager::QmlManager(QQuickView *view)
    : QObject(view)
    , m_view(view)
    , m_sceneType(MonitorSceneNone)
{
}

MonitorSceneType QmlManager::sceneType() const
{
    return m_sceneType;
}

void QmlManager::setProperty(const QString &name, const QVariant &value)
{
    m_view->rootObject()->setProperty(name.toUtf8().constData(), value);
}

void QmlManager::setScene(Kdenlive::MonitorId id, MonitorSceneType type, QSize profile, double profileStretch, QRect displayRect, double zoom, int duration)
{
    if (type == m_sceneType) {
        // Scene type already active
        return;
    }
    m_sceneType = type;
    QQuickItem *root = nullptr;
    m_view->rootContext()->setContextProperty("fixedFont", QFontDatabase::systemFont(QFontDatabase::FixedFont));
    double scalex = double(displayRect.width()) / profile.width() * zoom;
    double scaley = double(displayRect.height()) / profile.height() * zoom;
    switch (type) {
    case MonitorSceneGeometry:
        m_view->setSource(QUrl(QStringLiteral("qrc:/qml/kdenlivemonitoreffectscene.qml")));
        root = m_view->rootObject();
        QObject::connect(root, SIGNAL(effectChanged()), this, SLOT(effectRectChanged()), Qt::UniqueConnection);
        QObject::connect(root, SIGNAL(centersChanged()), this, SLOT(effectPolygonChanged()), Qt::UniqueConnection);
        root->setProperty("profile", QPoint(profile.width(), profile.height()));
        root->setProperty("framesize", QRect(0, 0, profile.width(), profile.height()));
        root->setProperty("scalex", scalex);
        root->setProperty("scaley", scaley);
        root->setProperty("center", displayRect.center());
        break;
    case MonitorSceneCorners:
        m_view->setSource(QUrl(QStringLiteral("qrc:/qml/kdenlivemonitorcornerscene.qml")));
        root = m_view->rootObject();
        QObject::connect(root, SIGNAL(effectPolygonChanged()), this, SLOT(effectPolygonChanged()), Qt::UniqueConnection);
        root->setProperty("profile", QPoint(profile.width(), profile.height()));
        root->setProperty("framesize", QRect(0, 0, profile.width(), profile.height()));
        root->setProperty("scalex", scalex);
        root->setProperty("scaley", scaley);
        root->setProperty("stretch", profileStretch);
        root->setProperty("center", displayRect.center());
        break;
    case MonitorSceneRoto:
        m_view->setSource(QUrl(QStringLiteral("qrc:/qml/kdenlivemonitorrotoscene.qml")));
        root = m_view->rootObject();
        QObject::connect(root, SIGNAL(effectPolygonChanged()), this, SLOT(effectRotoChanged()), Qt::UniqueConnection);
        root->setProperty("profile", QPoint(profile.width(), profile.height()));
        root->setProperty("framesize", QRect(0, 0, profile.width(), profile.height()));
        root->setProperty("scalex", scalex);
        root->setProperty("scaley", scaley);
        root->setProperty("stretch", profileStretch);
        root->setProperty("center", displayRect.center());
        break;
    case MonitorSplitTrack:
        m_view->setSource(QUrl(QStringLiteral("qrc:/qml/kdenlivemonitorsplittracks.qml")));
        root = m_view->rootObject();
        QObject::connect(root, SIGNAL(activateTrack(int)), this, SIGNAL(activateTrack(int)), Qt::UniqueConnection);
        root->setProperty("profile", QPoint(profile.width(), profile.height()));
        root->setProperty("framesize", QRect(0, 0, profile.width(), profile.height()));
        root->setProperty("scalex", scalex);
        root->setProperty("scaley", scaley);
        root->setProperty("stretch", profileStretch);
        root->setProperty("center", displayRect.center());
        break;
    case MonitorSceneSplit:
        m_view->setSource(QUrl(QStringLiteral("qrc:/qml/kdenlivemonitorsplit.qml")));
        root = m_view->rootObject();
        root->setProperty("profile", QPoint(profile.width(), profile.height()));
        root->setProperty("scalex", scalex);
        root->setProperty("scaley", scaley);
        break;
    case MonitorSceneRipple:
        m_view->setSource(QUrl(QStringLiteral("qrc:/qml/kdenlivemonitorripple.qml")));
        root = m_view->rootObject();
        break;
    default:         
        m_view->setSource(
            QUrl(id == Kdenlive::ClipMonitor ? QStringLiteral("qrc:/qml/kdenliveclipmonitor.qml") : QStringLiteral("qrc:/qml/kdenlivemonitor.qml")));
        root = m_view->rootObject();
        root->setProperty("profile", QPoint(profile.width(), profile.height()));
        root->setProperty("scalex", scalex);
        root->setProperty("scaley", scaley);
        if (id == Kdenlive::ClipMonitor) {
            // Apply the always show audio setting
            root->setProperty("permanentAudiothumb", KdenliveSettings::alwaysShowMonitorAudio());
        }
        break;
    }
    if (root && duration > 0) {
        root->setProperty("duration", duration);
    }
}

void QmlManager::effectRectChanged()
{
    if (!m_view->rootObject()) {
        return;
    }
    const QRect rect = m_view->rootObject()->property("framesize").toRect();
    emit effectChanged(rect);
}

void QmlManager::effectPolygonChanged()
{
    if (!m_view->rootObject()) {
        return;
    }
    QVariantList points = m_view->rootObject()->property("centerPoints").toList();
    emit effectPointsChanged(points);
}

void QmlManager::effectRotoChanged()
{
    if (!m_view->rootObject()) {
        return;
    }
    QVariantList points = m_view->rootObject()->property("centerPoints").toList();
    QVariantList controlPoints = m_view->rootObject()->property("centerPointsTypes").toList();
    // rotoscoping effect needs a list of
    QVariantList mix;
    mix.reserve(points.count());
    for (int i = 0; i < points.count(); i++) {
        mix << controlPoints.at(2 * i);
        mix << points.at(i);
        mix << controlPoints.at(2 * i + 1);
    }
    emit effectPointsChanged(mix);
}
