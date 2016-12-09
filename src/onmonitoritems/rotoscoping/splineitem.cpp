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
#include "kdenlivesettings.h"

#include <QGraphicsScene>
#include <QCursor>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsView>

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

SplineItem::SplineItem(const QList< BPoint > &points, QGraphicsItem *parent, QGraphicsScene *scene) :
    QGraphicsPathItem(parent),
    m_closed(true),
    m_editing(false)
{
    QPen framepen(Qt::SolidLine);
    framepen.setColor(Qt::yellow);
    setPen(framepen);
    setBrush(Qt::NoBrush);
    setAcceptHoverEvents(true);
    m_view = scene->views().at(0);
    initSpline(scene, points);
}

void SplineItem::initSpline(QGraphicsScene *scene, const QList< BPoint > &points)
{
    scene->addItem(this);
    setPoints(points);
}

int SplineItem::type() const
{
    return Type;
}

bool SplineItem::editing() const
{
    return m_editing;
}

void SplineItem::updateSpline(bool editing)
{
    QPainterPath path(qgraphicsitem_cast<BPointItem *>(childItems().at(0))->getPoint().p);

    BPoint p1, p2;
    for (int i = 0; i < childItems().count() - !m_closed; ++i) {
        int j = (i + 1) % childItems().count();
        p1 = qgraphicsitem_cast<BPointItem *>(childItems().at(i))->getPoint();
        p2 = qgraphicsitem_cast<BPointItem *>(childItems().at(j))->getPoint();
        path.cubicTo(p1.h2, p2.h1, p2.p);
    }
    setPath(path);

    m_editing = editing;

    if (m_closed && (!editing || KdenliveSettings::monitorscene_directupdate())) {
        emit changed(editing);
    }
}

QList<BPoint> SplineItem::getPoints() const
{
    QList<BPoint> points;
    foreach (QGraphicsItem *child, childItems()) {
        points << qgraphicsitem_cast<BPointItem *>(child)->getPoint();
    }
    return points;
}

void SplineItem::setPoints(const QList< BPoint > &points)
{
    if (points.count() < 2) {
        m_closed = false;
        grabMouse();
        return;
    } else if (!m_closed) {
        ungrabMouse();
        m_closed = true;
    }

    qDeleteAll(childItems());

    QPainterPath path(points.at(0).p);
    for (int i = 0; i < points.count(); ++i) {
        new BPointItem(points.at(i), this);
        int j = (i + 1) % points.count();
        path.cubicTo(points.at(i).h2, points.at(j).h1, points.at(j).p);
    }
    setPath(path);
}

void SplineItem::removeChild(QGraphicsItem *child)
{
    if (childItems().count() > 2) {
        scene()->removeItem(child);
        delete child;
        updateSpline();
    }
}

void SplineItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    event->setAccepted(m_closed);
    QGraphicsItem::mousePressEvent(event);

    if (event->isAccepted()) {
        return;
    }

    if (m_closed) {
        qreal size = 12 / m_view->matrix().m11();
        QRectF r(event->scenePos() - QPointF(size / 2, size / 2), QSizeF(size, size));
        if (event->button() == Qt::LeftButton && path().intersects(r) && !path().contains(r)) {
            double t = 0;
            BPointItem *i1, *i2;
            BPoint p, p1, p2;
            int ix = getClosestPointOnCurve(event->scenePos(), &t);
            i1 = qgraphicsitem_cast<BPointItem *>(childItems().at(ix));
            i2 = qgraphicsitem_cast<BPointItem *>(childItems().at((ix + 1) % childItems().count()));
            p1 = i1->getPoint();
            p2 = i2->getPoint();

            deCasteljau(&p1, &p2, &p, t);

            i1->setPoint(p1);
            i2->setPoint(p2);
            BPointItem *i = new BPointItem(p, this);
            i->stackBefore(i2);
            updateSpline();
        }
    } else {
        bool close = false;
        QList<QGraphicsItem *> items = childItems();
        if (items.count() > 1) {
            BPointItem *bp = qgraphicsitem_cast<BPointItem *>(items.at(0));
            int selectionType = bp->getSelection(mapToItem(bp, event->pos()));
            // since h1 == p we need to check for both
            if (selectionType == 0 || selectionType == 1) {
                close = true;
            }
        }

        if (close || event->button() == Qt::RightButton) {
            if (items.count() > 1) {
                // close the spline
                BPointItem *i1 = qgraphicsitem_cast<BPointItem *>(items.first());
                BPointItem *i2 = qgraphicsitem_cast<BPointItem *>(items.last());
                BPoint p1 = i1->getPoint();
                BPoint p2 = i2->getPoint();
                p1.h1 = QLineF(p1.p, p2.p).pointAt(.2);
                p2.h2 = QLineF(p1.p, p2.p).pointAt(.8);
                i1->setPoint(p1);
                i2->setPoint(p2);
                m_closed = true;
                ungrabMouse();
                updateSpline();
            }
        } else if (event->modifiers() == Qt::NoModifier) {
            BPoint p;
            p.p = p.h1 = p.h2 = event->scenePos();
            if (items.count()) {
                BPointItem *i = qgraphicsitem_cast<BPointItem *>(items.last());
                BPoint prev = i->getPoint();
                prev.h2 = QLineF(prev.p, p.p).pointAt(.2);
                p.h1 = QLineF(prev.p, p.p).pointAt(.8);
                i->setPoint(prev);
            }
            new BPointItem(p, this);
            updateSpline();
        }
    }
}

void SplineItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseMoveEvent(event);
}

void SplineItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    QGraphicsItem::mouseReleaseEvent(event);
}

void SplineItem::hoverMoveEvent(QGraphicsSceneHoverEvent *event)
{
    QGraphicsItem::hoverMoveEvent(event);

    qreal size = 12 / m_view->matrix().m11();
    QRectF r(event->scenePos() - QPointF(size / 2, size / 2), QSizeF(size, size));
    if (path().intersects(r) && !path().contains(r)) {
        setCursor(QCursor(Qt::PointingHandCursor));
    } else {
        unsetCursor();
    }
}

int SplineItem::getClosestPointOnCurve(const QPointF &point, double *tFinal)
{
    // TODO: proper minDiff
    qreal diff = 10000, param = 0;
    BPoint p1, p2;
    int curveSegment = 0;
    QList<QGraphicsItem *> items = childItems();
    for (int i = 0; i < items.count(); ++i) {
        int j = (i + 1) % items.count();
        p1 = qgraphicsitem_cast<BPointItem *>(items.at(i))->getPoint();
        p2 = qgraphicsitem_cast<BPointItem *>(items.at(j))->getPoint();
        QPolygonF bounding = QPolygonF() << p1.p << p1.h2 << p2.h1 << p2.p;
        QPointF cl = closestPointInRect(point, bounding.boundingRect());
        qreal d = (point - cl).manhattanLength();
        if (d > diff) {
            continue;
        }

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

