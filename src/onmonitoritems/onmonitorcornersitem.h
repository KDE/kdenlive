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

#ifndef ONMONITORCORNERSITEM_H
#define ONMONITORCORNERSITEM_H

#include <QGraphicsPolygonItem>

class QGraphicsView;

class OnMonitorCornersItem : public QObject, public QGraphicsPolygonItem
{
    Q_OBJECT
public:
    explicit OnMonitorCornersItem(QGraphicsItem *parent = nullptr);

    enum cornersActions { Corner, Move, MoveSide, NoAction };
    /** @brief Gets The action mode for the area @param pos +- 4. */
    cornersActions getMode(const QPointF &pos, int *corner);

    /** @brief Reimplemented to draw the handles. */
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) Q_DECL_OVERRIDE;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) Q_DECL_OVERRIDE;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) Q_DECL_OVERRIDE;

private:
    /** @brief Returns the centroid (= 'center of mass') of this polygon. */
    QPointF getCentroid();

    /** @brief Returns the points of this polygon but sorted clockwise. */
    QList<QPointF> sortedClockwise();

    /** @brief Tries to get the view of the scene. */
    bool getView();

    cornersActions m_mode;
    /** Number of the selected corner or if in MoveSide mode number of the first corner on this side */
    int m_selectedCorner;
    QPointF m_lastPoint;
    bool m_modified;

    QGraphicsView *m_view;

signals:
    void changed();
};

#endif
