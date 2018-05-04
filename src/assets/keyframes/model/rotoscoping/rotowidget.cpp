/*
Copyright (C) 2018  Jean-Baptiste Mardelle <jb@kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of
the License or (at your option) version 3 or any later version
accepted by the membership of KDE e.V. (or its successor approved
by the membership of KDE e.V.), which shall act as a proxy
defined in Section 14 of version 3 of the license.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "rotowidget.hpp"
#include "gentime.h"
#include "monitor/monitor.h"

#include <QSize>

RotoWidget::RotoWidget(Monitor *monitor, QPersistentModelIndex index, QWidget *parent)
    : QWidget(parent)
    , m_monitor(monitor)
    , m_index(index)
    , m_active(false)
{
}

bool RotoWidget::connectMonitor(bool activate)
{
    if (activate == m_active) {
        return false;
    }
    m_active = activate;
    if (activate) {
        connect(m_monitor, &Monitor::effectPointsChanged, this, &RotoWidget::slotUpdateRotoMonitor, Qt::UniqueConnection);
    } else {
        disconnect(m_monitor, &Monitor::effectPointsChanged, this, &RotoWidget::slotUpdateRotoMonitor);
    }
    return m_active;
}

void RotoWidget::slotUpdateRotoMonitor(const QVariantList &v)
{
    emit updateRotoKeyframe(m_index, v);
}


QVariant RotoWidget::getSpline(QVariant value, const QSize frame)
{
    QList<BPoint> bPoints;
    const QVariantList points = value.toList();
    for (int i = 0; i < points.size() / 3; i++) {
        BPoint b(points.at(3 * i).toPointF(), points.at(3 * i + 1).toPointF(), points.at(3 * i + 2).toPointF());
        bPoints << b;
    }
    QList<QVariant> vlist;
    foreach (const BPoint &point, bPoints) {
        QList<QVariant> pl;
        for (int i = 0; i < 3; ++i) {
            pl << QVariant(QList<QVariant>() << QVariant(point[i].x() / frame.width()) << QVariant(point[i].y() / frame.height()));
        }
        vlist << QVariant(pl);
    }
    return vlist;
}

QList<BPoint> RotoWidget::getPoints(QVariant value, const QSize frame)
{
    QList<BPoint> points;
    QList<QVariant> data = value.toList();

    // skip tracking flag
    if (data.count() && data.at(0).canConvert(QVariant::String)) {
        data.removeFirst();
    }

    foreach (const QVariant &bpoint, data) {
        QList<QVariant> l = bpoint.toList();
        BPoint p;
        for (int i = 0; i < 3; ++i) {
            p[i] = QPointF(l.at(i).toList().at(0).toDouble() * frame.width(), l.at(i).toList().at(1).toDouble() * frame.height());
        }
        points << p;
    }
    return points;
}
