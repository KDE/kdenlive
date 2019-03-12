/***************************************************************************
 *   Copyright (C) 2011 by Till Theato (root@ttill.de)                     *
 *   This file is part of Kdenlive (www.kdenlive.org).                     *
 *                                                                         *
 *   Kdenlive is free software: you can redistribute it and/or modify      *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation, either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Kdenlive is distributed in the hope that it will be useful,           *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with Kdenlive.  If not, see <http://www.gnu.org/licenses/>.     *
 ***************************************************************************/

#include "bpoint.h"

#include <QLineF>

BPoint::BPoint()
    : h1(-1, -1)
    , p(-1, -1)
    , h2(-1, -1)

{
}

BPoint::BPoint(const QPointF &handle1, const QPointF &point, const QPointF &handle2)
    : h1(handle1)
    , p(point)
    , h2(handle2)
{
    autoSetLinked();
}

QPointF &BPoint::operator[](int i)
{
    return i == 0 ? h1 : (i == 1 ? p : h2);
}

const QPointF &BPoint::operator[](int i) const
{
    return i == 0 ? h1 : (i == 1 ? p : h2);
}

bool BPoint::operator==(const BPoint &point) const
{
    return point.h1 == h1 && point.p == p && point.h2 == h2;
}

void BPoint::setP(const QPointF &point, bool updateHandles)
{
    QPointF offset = point - p;
    p = point;
    if (updateHandles) {
        h1 += offset;
        h2 += offset;
    }
}

void BPoint::setH1(const QPointF &handle1)
{
    h1 = handle1;
    if (handlesLinked) {
        qreal angle = QLineF(h1, p).angle();
        QLineF l = QLineF(p, h2);
        l.setAngle(angle);
        h2 = l.p2();
    }
}

void BPoint::setH2(const QPointF &handle2)
{
    h2 = handle2;
    if (handlesLinked) {
        qreal angle = QLineF(h2, p).angle();
        QLineF l = QLineF(p, h1);
        l.setAngle(angle);
        h1 = l.p2();
    }
}

void BPoint::autoSetLinked()
{
    // sometimes the angle is returned as 360°
    // due to rounding problems the angle is sometimes not quite 0
    qreal angle = QLineF(h1, p).angleTo(QLineF(p, h2));
    handlesLinked = angle < 1e-3 || qRound(angle) == 360;
}

void BPoint::setHandlesLinked(bool linked)
{
    handlesLinked = linked;
    if (linked) {
        // we force recomputing one of the handles
        setH1(h1);
    }
}
