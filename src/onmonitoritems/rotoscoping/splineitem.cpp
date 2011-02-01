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
#include "nearestpoint.h"

#include <QGraphicsScene>
#include <QCursor>
#include <QGraphicsSceneMouseEvent>


inline QPointF closestPointInRect(QPointF point, QRectF rect)
{
    QPointF closest;
    rect = rect.normalized();
    closest.setX(qBound<qreal>(rect.left(), point.x(), rect.right()));
    closest.setY(qBound<qreal>(rect.top(), point.y(), rect.bottom()));
    return closest;
}

void deCasteljau(BPoint *p1, BPoint *p2, BPoint *res, double t)
{
    QPointF ab, bc, cd;

    ab = QLineF(p1->p, p1->h2).pointAt(t);
    bc = QLineF(p1->h2, p2->h1).pointAt(t);
    cd = QLineF(p2->h1, p2->p).pointAt(t);

    res->h1 = QLineF(ab, bc).pointAt(t);
    res->h2 = QLineF(bc, cd).pointAt(t);
    res->p = QLineF(res->h1, res->h2).pointAt(t);

    p1->h2 = ab;
    p2->h1 = cd;
}


SplineItem::SplineItem(const QList< BPoint >& points, QGraphicsItem* parent, QGraphicsScene *scene) :
    QGraphicsPathItem(parent, scene)
{
    QPen framepen(Qt::SolidLine);
    framepen.setColor(Qt::yellow);
    setPen(framepen);
    setBrush(Qt::NoBrush);
    setAcceptHoverEvents(true);

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

void SplineItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsItem::mousePressEvent(event);

    if (!event->isAccepted()) {
        QRectF r(event->scenePos() - QPointF(6, 6), QSizeF(12, 12));
        if (path().intersects(r) && !path().contains(r)) {
            double t = 0;
            BPointItem *i, *i1, *i2;
            BPoint p, p1, p2;
            int ix = getClosestPointOnCurve(event->scenePos(), &t);
            i1 = qgraphicsitem_cast<BPointItem *>(childItems().at(ix));
            i2 = qgraphicsitem_cast<BPointItem *>(childItems().at((ix + 1) % childItems().count()));
            p1 = i1->getPoint();
            p2 = i2->getPoint();

            deCasteljau(&p1, &p2, &p, t);

            i1->setPoint(p1);
            i2->setPoint(p2);

            i = new BPointItem(p, this);
            // TODO: make it work with Qt < 4.6
#if QT_VERSION >= 0x040600
            i->stackBefore(i2);
#endif
        }
    }
}

void SplineItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsItem::mouseMoveEvent(event);
}

void SplineItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsItem::mouseReleaseEvent(event);
}

void SplineItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
{
    QGraphicsItem::hoverMoveEvent(event);

    QRectF r(event->scenePos() - QPointF(6, 6), QSizeF(12, 12));
    if (path().intersects(r) && !path().contains(r))
        setCursor(QCursor(Qt::PointingHandCursor));
    else
        unsetCursor();
}

int SplineItem::getClosestPointOnCurve(QPointF point, double *tFinal)
{
    // TODO: proper minDiff
    qreal diff = 10000, param = 0;
    BPoint p1, p2;
    int curveSegment = 0, j;
    for (int i = 0; i < childItems().count(); ++i) {
        j = (i + 1) % childItems().count();
        p1 = qgraphicsitem_cast<BPointItem *>(childItems().at(i))->getPoint();
        p2 = qgraphicsitem_cast<BPointItem *>(childItems().at(j))->getPoint();
        QPolygonF bounding = QPolygonF() << p1.p << p1.h2 << p2.h1 << p2.p;
        QPointF cl = closestPointInRect(point, bounding.boundingRect());
        qreal d = (point - cl).manhattanLength();

        if (d > diff)
            continue;

        Point2 b[4], p;
        double t;

        b[0].x = p1.p.x();
        b[0].y = p1.p.y();
        b[1].x = p1.h2.x();
        b[1].y = p1.h2.y();
        b[2].x = p2.h1.x();
        b[2].y = p2.h1.y();
        b[3].x = p2.p.x();
        b[3].y = p2.p.y();

        p.x = point.x();
        p.y = point.y();

        Point2 n = NearestPointOnCurve(p, b, &t);
        cl.setX(n.x);
        cl.setY(n.y);

        d = (point - cl).manhattanLength();
        if (d < diff) {
            diff = d;
            param = t;
            curveSegment = i;
        }
    }

    *tFinal = param;
    return curveSegment;
}

#include "splineitem.moc"
