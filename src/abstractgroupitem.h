/***************************************************************************
 *   Copyright (C) 2008 by Marco Gittler (g.marco@freenet.de)              *
 *   Copyright (C) 2008 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
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

#ifndef ABSTRACTGROUPITEM
#define ABSTRACTGROUPITEM

#include "definitions.h"
#include "gentime.h"

#include <QGraphicsItemGroup>
#include <QGraphicsSceneMouseEvent>

class CustomTrackScene;
class QGraphicsSceneMouseEvent;

class AbstractGroupItem : public QObject , public QGraphicsItemGroup
{
    Q_OBJECT
public:
    AbstractGroupItem(double fps);
    virtual int type() const;
    CustomTrackScene* projectScene();
    void addItem(QGraphicsItem * item);
    int track() const;
//    ItemInfo info() const;

protected:
    virtual QVariant itemChange(GraphicsItemChange change, const QVariant &value);
    virtual void paint(QPainter *, const QStyleOptionGraphicsItem *, QWidget *);
    virtual void dragEnterEvent(QGraphicsSceneDragDropEvent *event);
    virtual void dragLeaveEvent(QGraphicsSceneDragDropEvent *event);
    virtual void dropEvent(QGraphicsSceneDragDropEvent *event);
    virtual void mousePressEvent(QGraphicsSceneMouseEvent * event);

private:
    QPainterPath groupShape(QPointF);
    void fixItemRect();
};

#endif
