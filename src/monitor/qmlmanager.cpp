/*
    SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle <jb@kdenlive.org>
    This file is part of Kdenlive. See www.kdenlive.org.

    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "qmlmanager.h"
#include "kdenlivesettings.h"
#include "monitor.h"

#include <QFontDatabase>
#include <QQmlContext>
#include <QQuickItem>
#include <QQuickWidget>

QmlManager::QmlManager(QQuickWidget *view, Monitor *monitor)
    : QObject(monitor)
    , m_view(view)
    , m_monitor(monitor)
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

void QmlManager::blockSceneChange(bool block)
{
    m_sceneChangeBlocked = block;
}

bool QmlManager::setScene(Kdenlive::MonitorId id, MonitorSceneType type, QSize profile, double profileStretch, QRect displayRect, double zoom, int duration)
{
    if (type == m_sceneType || m_sceneChangeBlocked) {
        // Scene type already active
        return false;
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
        QObject::connect(root, SIGNAL(effectChanged(QRectF)), m_monitor, SIGNAL(effectChanged(QRectF)), Qt::UniqueConnection);
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
        QObject::connect(root, SIGNAL(effectPolygonChanged(QVariant, QVariant)), this, SLOT(effectRotoChanged(QVariant, QVariant)), Qt::UniqueConnection);
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
        root->setProperty("center", displayRect.center());
        break;
    case MonitorSceneTrimming:
        m_view->setSource(QUrl(QStringLiteral("qrc:/qml/kdenlivemonitortrimming.qml")));
        root = m_view->rootObject();
        break;
    case MonitorSceneAutoMask:
        m_view->setSource(QUrl(QStringLiteral("qrc:/qml/kdenlivemonitorautomask.qml")));
        root = m_view->rootObject();
        QObject::connect(root, SIGNAL(moveControlPoint(int, double, double)), m_monitor, SLOT(moveControlPoint(int, double, double)), Qt::UniqueConnection);
        QObject::connect(root, SIGNAL(addControlPoint(double, double, bool, bool)), m_monitor, SLOT(addControlPoint(double, double, bool, bool)),
                         Qt::UniqueConnection);
        QObject::connect(root, SIGNAL(addControlRect(double, double, double, double, bool)), m_monitor,
                         SLOT(addControlRect(double, double, double, double, bool)), Qt::UniqueConnection);
        QObject::connect(root, SIGNAL(generateMask()), m_monitor, SIGNAL(generateMask()), Qt::UniqueConnection);
        QObject::connect(root, SIGNAL(exitMaskPreview()), m_monitor, SIGNAL(disablePreviewMask()), Qt::UniqueConnection);
        root->setProperty("profile", QPoint(profile.width(), profile.height()));
        root->setProperty("scalex", scalex);
        root->setProperty("scaley", scaley);
        root->setProperty("center", displayRect.center());
        root->setProperty("maskStart", m_monitor->getZoneStart());
        root->setProperty("maskEnd", m_monitor->getZoneEnd());
        break;
    default:
        if (id == Kdenlive::ClipMonitor) {
            m_view->setSource(QUrl(QStringLiteral("qrc:/qml/kdenliveclipmonitor.qml")));
        } else {
            m_view->setSource(QUrl(QStringLiteral("qrc:/qml/kdenlivemonitor.qml")));
        }
        root = m_view->rootObject();
        root->setProperty("profile", QPoint(profile.width(), profile.height()));
        root->setProperty("scalex", scalex);
        root->setProperty("scaley", scaley);
        root->setProperty("center", displayRect.center());
        if (id == Kdenlive::ClipMonitor) {
            // Apply the always show audio setting
            root->setProperty("permanentAudiothumb", KdenliveSettings::alwaysShowMonitorAudio());
        }
        break;
    }
    if (root && duration > 0) {
        root->setProperty("duration", duration);
    }
    return true;
}

void QmlManager::effectPolygonChanged()
{
    if (!m_view->rootObject()) {
        return;
    }
    const QVariantList points = m_view->rootObject()->property("centerPoints").toList();
    Q_EMIT effectPointsChanged(points);
}

void QmlManager::effectRotoChanged(const QVariant &pts, const QVariant &centers)
{
    if (!m_view->rootObject()) {
        return;
    }
    QVariantList points = pts.toList();
    QVariantList controlPoints = centers.toList();
    if (2 * points.size() != controlPoints.size()) {
        // Mismatch, abort
        return;
    }
    // rotoscoping effect needs a list of
    QVariantList mix;
    mix.reserve(points.count());
    for (int i = 0; i < points.count(); i++) {
        mix << controlPoints.at(2 * i);
        mix << points.at(i);
        mix << controlPoints.at(2 * i + 1);
    }
    Q_EMIT effectPointsChanged(mix);
}
