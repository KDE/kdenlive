/*
    SPDX-FileCopyrightText: 2016 Jean-Baptiste Mardelle (jb@kdenlive.org)
    This file is part of Kdenlive. See www.kdenlive.org.

    SPDX-License-Identifier: LicenseRef-KDE-Accepted-GPL
*/

#ifndef QMLMANAGER_H
#define QMLMANAGER_H

#include "definitions.h"

class QQuickView;

/** @class QmlManager
    @brief Manages all Qml monitor overlays
    @author Jean-Baptiste Mardelle
 */
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
