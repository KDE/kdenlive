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

#include "abstractgroupitem.h"
#include "abstractclipitem.h"
#include "kdenlivesettings.h"
#include "customtrackscene.h"

#include <KDebug>

#include <QPainter>
#include <QStyleOptionGraphicsItem>


AbstractGroupItem::AbstractGroupItem(double fps): QGraphicsItemGroup(), m_fps(fps) {
    setZValue(2);
    setFlags(QGraphicsItem::ItemClipsToShape | QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
}

int AbstractGroupItem::type() const {
    return GROUPWIDGET;
}

const int AbstractGroupItem::track() const {
    return (int)(scenePos().y() / KdenliveSettings::trackheight());
}

CustomTrackScene* AbstractGroupItem::projectScene() {
    if (scene()) return static_cast <CustomTrackScene*>(scene());
    return NULL;
}

QPainterPath AbstractGroupItem::groupShape(QPointF offset) {
    QPainterPath path;
    QList<QGraphicsItem *> children = childItems();
    for (int i = 0; i < children.count(); i++) {
        if (children.at(i)->type() == AVWIDGET) {
            QRectF r(children.at(i)->sceneBoundingRect());
            r.translate(offset);
            path.addRect(r);
        }
    }
    return path;
}

void AbstractGroupItem::addItem(QGraphicsItem * item) {
    addToGroup(item);
    //fixItemRect();
}

void AbstractGroupItem::fixItemRect() {
    QPointF start = boundingRect().topLeft();
    if (start != QPointF(0, 0)) {
        translate(0 - start.x(), 0 - start.y());
        setPos(start);
    }
}

// virtual
void AbstractGroupItem::paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *) {
    p->fillRect(option->exposedRect, QColor(200, 100, 100, 100));
}

//virtual
QVariant AbstractGroupItem::itemChange(GraphicsItemChange change, const QVariant &value) {
    if (change == ItemPositionChange && scene()) {
        // calculate new position.
        const int trackHeight = KdenliveSettings::trackheight();
        QPointF newPos = value.toPointF();
        int xpos = projectScene()->getSnapPointForPos((int) newPos.x(), KdenliveSettings::snaptopoints());
        xpos = qMax(xpos, 0);
        newPos.setX(xpos);

        QPointF start = pos();
        int startTrack = (start.y() + trackHeight / 2) / trackHeight;
        int newTrack = (newPos.y()) / trackHeight;
        //kDebug()<<"// GROUP NEW T:"<<newTrack<<",START T:"<<startTrack<<",MAX:"<<projectScene()->tracksCount() - 1;
        newTrack = qMin(newTrack, projectScene()->tracksCount() - (int)(boundingRect().height() + 5) / trackHeight);
        newTrack = qMax(newTrack, 0);

        // Check if top item is a clip or a transition
        int offset = 0;
        int topTrack = -1;
        QList<QGraphicsItem *> children = childItems();
        for (int i = 0; i < children.count(); i++) {
            int currentTrack = (int)(children.at(i)->scenePos().y() / trackHeight);
            if (children.at(i)->type() == AVWIDGET) {
                if (topTrack == -1 || currentTrack <= topTrack) {
                    offset = 0;
                    topTrack = currentTrack;
                }
            } else if (children.at(i)->type() == TRANSITIONWIDGET) {
                if (topTrack == -1 || currentTrack < topTrack) {
                    offset = (int)(trackHeight / 3 * 2 - 1);
                    topTrack = currentTrack;
                }
            }
        }
        newPos.setY((int)((newTrack) * trackHeight) + offset);
        if (newPos == start) return start;

        if (newPos.x() < 0) {
            // If group goes below 0, adjust position to 0
            return QPointF(pos().x() - start.x(), pos().y());
        }

        QPainterPath shape = groupShape(newPos - pos());
        QList<QGraphicsItem*> collindingItems = scene()->items(shape, Qt::IntersectsItemShape);
        for (int i = 0; i < children.count(); i++) {
            collindingItems.removeAll(children.at(i));
        }
        if (collindingItems.isEmpty()) return newPos;
        else {
            bool forwardMove = newPos.x() > start.x();
            int offset = 0;
            for (int i = 0; i < collindingItems.count(); i++) {
                QGraphicsItem *collision = collindingItems.at(i);
                if (collision->type() == AVWIDGET) {
                    // Collision
                    //kDebug()<<"// COLLISION WIT:"<<collision->sceneBoundingRect();
                    if (newPos.y() != start.y()) {
                        // Track change results in collision, restore original position
                        return start;
                    }
                    AbstractClipItem *item = static_cast <AbstractClipItem *>(collision);
                    if (forwardMove) {
                        // Moving forward, determine best pos
                        QPainterPath clipPath;
                        clipPath.addRect(item->sceneBoundingRect());
                        QPainterPath res = shape.intersected(clipPath);
                        offset = qMax(offset, (int)(res.boundingRect().width() + 0.5));
                    } else {
                        // Moving backward, determine best pos
                        QPainterPath clipPath;
                        clipPath.addRect(item->sceneBoundingRect());
                        QPainterPath res = shape.intersected(clipPath);
                        offset = qMax(offset, (int)(res.boundingRect().width() + 0.5));
                    }
                }
            }
            if (offset > 0) {
                if (forwardMove) {
                    newPos.setX(newPos.x() - offset);
                } else {
                    newPos.setX(newPos.x() + offset);
                }
                // If there is still a collision after our position adjust, restore original pos
                collindingItems = scene()->items(groupShape(newPos - pos()), Qt::IntersectsItemShape);
                for (int i = 0; i < children.count(); i++) {
                    collindingItems.removeAll(children.at(i));
                }
                for (int i = 0; i < collindingItems.count(); i++)
                    if (collindingItems.at(i)->type() == AVWIDGET) return pos();
            }
            return newPos;
        }
    }
    return QGraphicsItemGroup::itemChange(change, value);
}

