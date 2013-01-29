/***************************************************************************
 *   Copyright (C) 2010 by Till Theato (root@ttill.de)                     *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


#ifndef ONMONITORPATHITEM_H
#define ONMONITORPATHITEM_H


#include <QtCore>
#include <QGraphicsPathItem>

class QGraphicsView;

namespace Mlt {
class Geometry;
}


class OnMonitorPathItem : public QObject, public QGraphicsPathItem
{
    Q_OBJECT
public:
    OnMonitorPathItem(double dar, QGraphicsItem *parent = 0);
    void setPoints(Mlt::Geometry *geometry);
    void getMode(QPointF pos);
    void rebuildShape();
    QList <QPointF> points() const;
    virtual QPainterPath shape () const;

    /** @brief Reimplemented to draw the handles. */
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0 );
    virtual QRectF boundingRect () const;

protected:
    //virtual void mousePressEvent(QGraphicsSceneMouseEvent *event);
    virtual void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    virtual void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);
    virtual void hoverMoveEvent(QGraphicsSceneHoverEvent *event);
    virtual void hoverLeaveEvent(QGraphicsSceneHoverEvent *event);
    virtual void hoverEnterEvent(QGraphicsSceneHoverEvent *event);

private:
    double m_dar;
    QList <QPointF> m_points;
    QPointF m_lastPoint;
    bool m_modified;
    QGraphicsView *m_view;
    int m_activePoint;
    QPainterPath m_shape;

    /** @brief Tries to get the view of the scene. */
    bool getView();

signals:
    void changed();
};

#endif
