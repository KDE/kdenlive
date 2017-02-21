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

#ifndef ONMONITORRECTITEM_H
#define ONMONITORRECTITEM_H

#include <QGraphicsRectItem>

class QGraphicsView;

enum rectActions { Move, ResizeTopLeft, ResizeBottomLeft, ResizeTopRight, ResizeBottomRight, ResizeLeft, ResizeRight, ResizeTop, ResizeBottom, NoAction };

class OnMonitorRectItem : public QObject, public QGraphicsRectItem
{
    Q_OBJECT
public:
    OnMonitorRectItem(const QRectF &rect, double dar, QGraphicsItem *parent = nullptr);

    /** @brief Gets The action mode for the area @param pos +- 4.
     * e.g. pos(0,0) returns ResizeTopLeft */
    rectActions getMode(const QPointF &pos);

    /** @brief Reimplemented to draw the handles. */
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = nullptr) Q_DECL_OVERRIDE;

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event) Q_DECL_OVERRIDE;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event) Q_DECL_OVERRIDE;
    void hoverMoveEvent(QGraphicsSceneHoverEvent *event) Q_DECL_OVERRIDE;

private:
    double m_dar;
    rectActions m_mode;
    QRectF m_oldRect;
    QPointF m_lastPoint;
    bool m_modified;

    QGraphicsView *m_view;

    /** @brief Tries to get the view of the scene. */
    bool getView();

signals:
    void changed();
};

#endif
