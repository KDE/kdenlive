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

#include <QtCore>
#include <QGraphicsRectItem>

enum rectActions { Move, ResizeTopLeft, ResizeBottomLeft, ResizeTopRight, ResizeBottomRight, ResizeLeft, ResizeRight, ResizeTop, ResizeBottom, NoAction };

class OnMonitorRectItem : public QObject, public QGraphicsRectItem
{
    Q_OBJECT
public:
    OnMonitorRectItem(const QRectF &rect, QGraphicsItem *parent = 0);

    /** @brief Enables/Disables the ability to modify the item.
     * @param enabled (default = true) */
    void setEnabled(bool enabled = true);

    /** @brief Gets The action mode for the area @param pos +- 4.
     * e.g. pos(0,0) returns ResizeTopLeft */
    rectActions getMode(QPoint pos);
    
    enum { Type = UserType + 1};
    /** @brief Reimplemented to make sure casting works. */
    int type() const;

    /** @brief Reimplemented to draw the handles. */
    virtual void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0 );

public slots:
    /** @brief Saves current mouse position and mode. */
    void slotMousePressed(QGraphicsSceneMouseEvent *event);
    /** @brief emits actionFinished signal if item was modified. */
    void slotMouseReleased(QGraphicsSceneMouseEvent *event);
    /** @brief Modifies item according to mouse position and mode. */
    void slotMouseMoved(QGraphicsSceneMouseEvent *event);

private:
    rectActions m_mode;
    QPointF m_clickPoint;
    bool m_enabled;
    bool m_modified;

signals:
    void actionFinished();
    void setCursor(const QCursor &);
};

#endif
