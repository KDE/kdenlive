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

#ifndef SPLINEITEM_H
#define SPLINEITEM_H

#include <QGraphicsPathItem>

class BPoint;
class QGraphicsView;

class SplineItem : public QObject, public QGraphicsPathItem
{
    Q_OBJECT

public:
    explicit SplineItem(const QList<BPoint> &points, QGraphicsItem *parent = nullptr, QGraphicsScene *scene = nullptr);

    enum { Type = UserType + 10 };

    int type() const override;

    bool editing() const;
    void initSpline(QGraphicsScene *scene, const QList< BPoint > &points);
    void updateSpline(bool editing = false);
    QList<BPoint> getPoints() const;
    void setPoints(const QList<BPoint> &points);

    void removeChild(QGraphicsItem *child);

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) override;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) override;

private:
    int getClosestPointOnCurve(const QPointF &point, double *tFinal);

    bool m_closed;
    bool m_editing;
    QGraphicsView *m_view;

signals:
    void changed(bool editing);
};

#endif
