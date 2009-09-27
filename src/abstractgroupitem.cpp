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
#include "customtrackview.h"

#include <KDebug>

#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QDomDocument>
#include <QMimeData>
#include <QGraphicsSceneMouseEvent>

AbstractGroupItem::AbstractGroupItem(double /* fps */) :
        QObject(),
        QGraphicsItemGroup()
{
    setZValue(1);
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
    setAcceptDrops(true);
}

int AbstractGroupItem::type() const
{
    return GROUPWIDGET;
}

int AbstractGroupItem::track() const
{
    return (int)(scenePos().y() / KdenliveSettings::trackheight());
}

CustomTrackScene* AbstractGroupItem::projectScene()
{
    if (scene()) return static_cast <CustomTrackScene*>(scene());
    return NULL;
}

QPainterPath AbstractGroupItem::clipGroupShape(QPointF offset) const
{
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

QPainterPath AbstractGroupItem::transitionGroupShape(QPointF offset) const
{
    QPainterPath path;
    QList<QGraphicsItem *> children = childItems();
    for (int i = 0; i < children.count(); i++) {
        if (children.at(i)->type() == TRANSITIONWIDGET) {
            QRectF r(children.at(i)->sceneBoundingRect());
            r.translate(offset);
            path.addRect(r);
        }
    }
    return path;
}

void AbstractGroupItem::addItem(QGraphicsItem * item)
{
    addToGroup(item);
    //fixItemRect();
}

void AbstractGroupItem::fixItemRect()
{
    QPointF start = boundingRect().topLeft();
    if (start != QPointF(0, 0)) {
        translate(0 - start.x(), 0 - start.y());
        setPos(start);
    }
}

/*ItemInfo AbstractGroupItem::info() const {
    ItemInfo itemInfo;
    itemInfo.startPos = m_startPos;
    itemInfo.track = m_track;
    return itemInfo;
}*/

// virtual
void AbstractGroupItem::paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *)
{
    const double scale = option->matrix.m11();
    QColor bgcolor(100, 100, 200, 100);
    QRectF bound = option->exposedRect.adjusted(0, 0, 1, 1);
    p->setClipRect(bound);
    p->fillRect(option->exposedRect, bgcolor);
    QPen pen = p->pen();
    pen.setColor(QColor(200, 90, 90));
    pen.setStyle(Qt::DashLine);
    pen.setWidthF(0.0);
    //pen.setCosmetic(true);
    p->setPen(pen);
    p->drawRect(boundingRect().adjusted(0, 0, - 1 / scale, 0));
}

//virtual
QVariant AbstractGroupItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == ItemPositionChange && scene() && parentItem() == 0) {
        // calculate new position.
        const int trackHeight = KdenliveSettings::trackheight();
        QPointF start = sceneBoundingRect().topLeft();
        QPointF newPos = value.toPointF();
        //kDebug()<<"REAL:"<<start.x()<<", PROPOSED:"<<(int)(start.x() - pos().x() + newPos.x());
        int xpos = projectScene()->getSnapPointForPos((int)(start.x() + newPos.x() - pos().x()), KdenliveSettings::snaptopoints());

        xpos = qMax(xpos, 0);
        //kDebug()<<"GRP XPOS:"<<xpos<<", START:"<<start.x()<<",NEW:"<<newPos.x()<<"; SCENE:"<<scenePos().x()<<",POS:"<<pos().x();
        newPos.setX((int)(pos().x() + xpos - (int) start.x()));

        //int startTrack = (start.y() + trackHeight / 2) / trackHeight;

        int realTrack = (start.y() + newPos.y() - pos().y()) / trackHeight;
        int proposedTrack = newPos.y() / trackHeight;

        int correctedTrack = qMin(realTrack, projectScene()->tracksCount() - (int)(boundingRect().height() + 5) / trackHeight);
        correctedTrack = qMax(correctedTrack, 0);

        proposedTrack += (correctedTrack - realTrack);

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
            } else if (children.at(i)->type() == GROUPWIDGET) {
                QList<QGraphicsItem *> subchildren = children.at(i)->childItems();
                bool clipGroup = false;
                for (int j = 0; j < subchildren.count(); j++) {
                    if (subchildren.at(j)->type() == AVWIDGET) {
                        clipGroup = true;
                        break;
                    }
                }
                if (clipGroup) {
                    if (topTrack == -1 || currentTrack <= topTrack) {
                        offset = 0;
                        topTrack = currentTrack;
                    }
                } else {
                    if (topTrack == -1 || currentTrack < topTrack) {
                        offset = (int)(trackHeight / 3 * 2 - 1);
                        topTrack = currentTrack;
                    }
                }
            }
        }
        newPos.setY((int)((proposedTrack) * trackHeight) + offset);
        //if (newPos == start) return start;

        /*if (newPos.x() < 0) {
            // If group goes below 0, adjust position to 0
            return QPointF(pos().x() - start.x(), pos().y());
        }*/

        QPainterPath shape = clipGroupShape(newPos - pos());
        QList<QGraphicsItem*> collindingItems = scene()->items(shape, Qt::IntersectsItemShape);
        for (int i = 0; i < children.count(); i++) {
            collindingItems.removeAll(children.at(i));
        }
        
        if (!collindingItems.isEmpty()) {
            bool forwardMove = xpos > start.x();
            int offset = 0;
            for (int i = 0; i < collindingItems.count(); i++) {
                QGraphicsItem *collision = collindingItems.at(i);
                if (collision->type() == AVWIDGET) {
                    // Collision
                    if (newPos.y() != pos().y()) {
                        // Track change results in collision, restore original position
                        return pos();
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
                collindingItems = scene()->items(clipGroupShape(newPos - pos()), Qt::IntersectsItemShape);
                for (int i = 0; i < children.count(); i++) {
                    collindingItems.removeAll(children.at(i));
                }
                for (int i = 0; i < collindingItems.count(); i++)
                    if (collindingItems.at(i)->type() == AVWIDGET) return pos();
            }
        }


        shape = transitionGroupShape(newPos - pos());
        collindingItems = scene()->items(shape, Qt::IntersectsItemShape);
        for (int i = 0; i < children.count(); i++) {
            collindingItems.removeAll(children.at(i));
        }
        if (collindingItems.isEmpty()) return newPos;
        else {
            bool forwardMove = xpos > start.x();
            int offset = 0;
            for (int i = 0; i < collindingItems.count(); i++) {
                QGraphicsItem *collision = collindingItems.at(i);
                if (collision->type() == TRANSITIONWIDGET) {
                    // Collision
                    if (newPos.y() != pos().y()) {
                        // Track change results in collision, restore original position
                        return pos();
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
                collindingItems = scene()->items(transitionGroupShape(newPos - pos()), Qt::IntersectsItemShape);
                for (int i = 0; i < children.count(); i++) {
                    collindingItems.removeAll(children.at(i));
                }
                for (int i = 0; i < collindingItems.count(); i++)
                    if (collindingItems.at(i)->type() == TRANSITIONWIDGET) return pos();
            }
        }
        return newPos;
    }
    return QGraphicsItemGroup::itemChange(change, value);
}

//virtual
void AbstractGroupItem::dropEvent(QGraphicsSceneDragDropEvent * event)
{
    QString effects = QString(event->mimeData()->data("kdenlive/effectslist"));
    QDomDocument doc;
    doc.setContent(effects, true);
    QDomElement e = doc.documentElement();
    CustomTrackView *view = (CustomTrackView *) scene()->views()[0];
    if (view) view->slotAddGroupEffect(e, this);
}

//virtual
void AbstractGroupItem::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    event->setAccepted(event->mimeData()->hasFormat("kdenlive/effectslist"));
}

void AbstractGroupItem::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
    Q_UNUSED(event);
}

// virtual
void AbstractGroupItem::mousePressEvent(QGraphicsSceneMouseEvent * event)
{
    if (event->modifiers() & Qt::ShiftModifier) {
        // User want to do a rectangle selection, so ignore the event to pass it to the view
        event->ignore();
    } else QGraphicsItem::mousePressEvent(event);
}
