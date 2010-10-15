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

#include "abstractonmonitoritem.h"

#include <QtCore>
#include <QGraphicsPolygonItem>

enum cornersActions { Corner1, Corner2, Corner3, Corner4, NoAction };

class OnMonitorCornersItem : public AbstractOnMonitorItem, public QGraphicsPolygonItem
{
    Q_OBJECT
public:
    OnMonitorCornersItem(MonitorScene *scene, QGraphicsItem *parent = 0);

    /** @brief Gets The action mode for the area @param pos +- 4. */
    cornersActions getMode(QPoint pos);
    
    /** @brief Reimplemented to draw the handles. */
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0 );

public slots:
    /** @brief Saves current mouse position and mode. */
    void slotMousePressed(QGraphicsSceneMouseEvent *event);
    /** @brief Modifies item according to mouse position and mode. */
    void slotMouseMoved(QGraphicsSceneMouseEvent *event);

private:
    cornersActions m_mode;
    QPointF m_clickPoint;
};

#endif
