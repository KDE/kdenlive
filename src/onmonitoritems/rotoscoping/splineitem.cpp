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

#include "splineitem.h"
#include "bpointitem.h"

#include <QGraphicsScene>

SplineItem::SplineItem(const QList< BPoint >& points, QGraphicsItem* parent, QGraphicsScene *scene) :
    QGraphicsPathItem(parent, scene)
{
    QPen framepen(Qt::SolidLine);
    framepen.setColor(Qt::yellow);
    setPen(framepen);
    setBrush(Qt::NoBrush);

    if (points.isEmpty())
        return;

    QPainterPath path(points.at(0).p);
    int j;
    for (int i = 0; i < points.count(); ++i) {
        new BPointItem(points.at(i), this);
        j = (i + 1) % points.count();
        path.cubicTo(points.at(i).h2, points.at(j).h1, points.at(j).p);
    }
    setPath(path);
}

int SplineItem::type() const
{
    return Type;
}

void SplineItem::updateSpline()
{
    QPainterPath path(qgraphicsitem_cast<BPointItem *>(childItems().at(0))->getPoint().p);

    BPoint p1, p2;
    int j;
    for (int i = 0; i < childItems().count(); ++i) {
        j = (i + 1) % childItems().count();
        p1 = qgraphicsitem_cast<BPointItem *>(childItems().at(i))->getPoint();
        p2 = qgraphicsitem_cast<BPointItem *>(childItems().at(j))->getPoint();
        path.cubicTo(p1.h2, p2.h1, p2.p);
    }
    setPath(path);

    emit changed();
}

QList <BPoint> SplineItem::getPoints()
{
    QList <BPoint> points;
    foreach (QGraphicsItem *child, childItems())
        points << qgraphicsitem_cast<BPointItem *>(child)->getPoint();
    return points;
}

#include "splineitem.moc"
