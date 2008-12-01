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

#include <QGraphicsScene>
#include <QGraphicsView>
#include <QScrollBar>
#include <QToolTip>

#include <KDebug>
#include <KLocale>

#include "abstractgroupitem.h"
#include "abstractclipitem.h"
#include "kdenlivesettings.h"
#include "customtrackscene.h"

AbstractGroupItem::AbstractGroupItem(double fps): QGraphicsItemGroup(), m_fps(fps) {
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


QPolygonF AbstractGroupItem::groupShape(QPointF offset) {
    QList<QGraphicsItem *> children = childItems();
    QPolygonF path;
    for (int i = 0; i < children.count(); i++) {
        if (children.at(i)->type() == AVWIDGET) {
            QPolygonF r = QPolygonF(children.at(i)->sceneBoundingRect());
            path = path.united(r);
        }
    }
    path.translate(offset);
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
void AbstractGroupItem::paint(QPainter *p, const QStyleOptionGraphicsItem *, QWidget *) {
    p->fillRect(boundingRect(), QColor(200, 100, 100, 100));
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

        //kDebug()<<"// GRP MOVE: "<<pos().y()<<"->"<<newPos.y();
        QPointF start = pos();//sceneBoundingRect().topLeft();
        int posx = start.x() + newPos.x(); //projectScene()->getSnapPointForPos(start.x() + sc.x(), KdenliveSettings::snaptopoints());

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
                kDebug() << "// CLIP ITEM TRK: " << currentTrack << "; POS: " << children.at(i)->scenePos().y();
                if (topTrack == -1 || currentTrack <= topTrack) {
                    offset = 0;
                    topTrack = currentTrack;
                }
            } else if (children.at(i)->type() == TRANSITIONWIDGET) {
                kDebug() << "// TRANS ITEM TRK: " << currentTrack << "; POS: " << children.at(i)->scenePos().y();
                if (topTrack == -1 || currentTrack < topTrack) {
                    offset = (int)(trackHeight / 3 * 2 - 1);
                    topTrack = currentTrack;
                }
            }
        }
        kDebug() << "// OFFSET: " << offset << "\n------------------------------------\n------------";

        newPos.setY((int)((newTrack) * trackHeight) + offset);

        //kDebug() << "------------------------------------GRUOP MOVE";

        if (start.x() + newPos.x() - pos().x() < 0) {
            // If group goes below 0, adjust position to 0
            return QPointF(pos().x() - start.x(), pos().y());
        }

        QPolygonF sceneShape = groupShape(newPos - pos());
        QList<QGraphicsItem*> collindingItems = scene()->items(sceneShape, Qt::IntersectsItemShape);
        for (int i = 0; i < children.count(); i++) {
            collindingItems.removeAll(children.at(i));
        }
        if (collindingItems.isEmpty()) return newPos;
        else {
            for (int i = 0; i < collindingItems.count(); i++) {
                QGraphicsItem *collision = collindingItems.at(i);
                if (collision->type() == AVWIDGET) {
                    // Collision
                    return pos();
                    //TODO: improve movement when collision happens
                    /*if (startTrack != newTrack) return pos();
                    if (collision->pos().x() > pos().x()) {
                    return QPointF(collision->sceneBoundingRect().x() - sceneBoundingRect().width() + pos().x() - start.x() - 1, newPos.y());
                    }*/
                }
            }
            return newPos;
        }

        //else posx -= startx;
        //posx = qMax(posx, 0);
        //newPos.setX(posx);
        //kDebug()<<"Y POS: "<<start.y() + newPos.y()<<"SCN MP: "<<sc;
        /*int newTrack = (start.y() + newPos.y()) / KdenliveSettings::trackheight();
        int oldTrack = (start.y() + pos().y()) / KdenliveSettings::trackheight();
        newPos.setY((newTrack) * KdenliveSettings::trackheight() - start.y() + 1);*/


        //if (start.y() + newPos.y() < 1)  newTrack = oldTrack;

        return newPos;

        // Only one clip is moving

        QList<QGraphicsItem*> items = scene()->items(sceneShape, Qt::IntersectsItemShape);


        if (!items.isEmpty()) {
            for (int i = 0; i < items.count(); i++) {
                if (items.at(i)->type() == AVWIDGET) {
                    // Collision!
                    //kDebug()<<"/// COLLISION WITH ITEM: "<<items.at(i)->sceneBoundingRect();
                    return pos();
                    QPointF otherPos = items.at(i)->pos();
                    if ((int) otherPos.y() != (int) pos().y()) return pos();
                    if (pos().x() < otherPos.x()) {
                        // move clip just before colliding clip
                        int npos = (static_cast < AbstractClipItem* >(items.at(i))->startPos()).frames(m_fps) - sceneBoundingRect().width();
                        newPos.setX(npos);
                    } else {
                        // get pos just after colliding clip
                        int npos = static_cast < AbstractClipItem* >(items.at(i))->endPos().frames(m_fps);
                        newPos.setX(npos);
                    }
                    return newPos;
                }
            }
        }
        return newPos;
    }
    return QGraphicsItem::itemChange(change, value);
}

