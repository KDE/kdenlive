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
#if QT_VERSION >= 0x040600
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
#endif
    setAcceptDrops(true);
    m_resizeInfos = QList <ItemInfo>();
}

int AbstractGroupItem::type() const
{
    return GROUPWIDGET;
}

int AbstractGroupItem::track() const
{
    return (int)(scenePos().y() / KdenliveSettings::trackheight());
}

void AbstractGroupItem::setItemLocked(bool locked)
{
    if (locked)
        setSelected(false);

    setFlag(QGraphicsItem::ItemIsMovable, !locked);
    setFlag(QGraphicsItem::ItemIsSelectable, !locked);

    foreach (QGraphicsItem *child, childItems())
        ((AbstractClipItem *)child)->setItemLocked(locked);
}

bool AbstractGroupItem::isItemLocked() const
{
    return !(flags() & (QGraphicsItem::ItemIsSelectable));
}

CustomTrackScene* AbstractGroupItem::projectScene()
{
    if (scene()) return static_cast <CustomTrackScene*>(scene());
    return NULL;
}

QPainterPath AbstractGroupItem::clipGroupShape(QPointF offset) const
{
    return groupShape(AVWIDGET, offset);
}

QPainterPath AbstractGroupItem::transitionGroupShape(QPointF offset) const
{
    return groupShape(TRANSITIONWIDGET, offset);
}

QPainterPath AbstractGroupItem::groupShape(GRAPHICSRECTITEM type, QPointF offset) const
{
    QPainterPath path;
    QList<QGraphicsItem *> children = childItems();
    for (int i = 0; i < children.count(); i++) {
        if (children.at(i)->type() == (int)type) {
            QRectF r(children.at(i)->sceneBoundingRect());
            r.translate(offset);
            path.addRect(r);
        } else if (children.at(i)->type() == GROUPWIDGET) {
            QList<QGraphicsItem *> subchildren = children.at(i)->childItems();
            for (int j = 0; j < subchildren.count(); j++) {
                if (subchildren.at(j)->type() == (int)type) {
                    QRectF r(subchildren.at(j)->sceneBoundingRect());
                    r.translate(offset);
                    path.addRect(r);
                }
            }
        }
    }
    return path;
}

void AbstractGroupItem::addItem(QGraphicsItem * item)
{
    addToGroup(item);
    item->setFlag(QGraphicsItem::ItemIsMovable, false);
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
    QColor bgcolor(100, 100, 200, 100);
    QRectF bound = option->exposedRect.adjusted(0, 0, 1, 1);
    p->setClipRect(bound);
    const QRectF mapped = p->worldTransform().mapRect(option->exposedRect);
    p->setWorldMatrixEnabled(false);
    p->setBrush(bgcolor);
    QPen pen = p->pen();
    pen.setColor(QColor(200, 90, 90));
    pen.setStyle(Qt::DashLine);
    pen.setWidthF(0.0);
    p->setPen(pen);
    p->drawRoundedRect(mapped, 3, 3);
}

//virtual
QVariant AbstractGroupItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == QGraphicsItem::ItemSelectedChange) {
        if (value.toBool()) setZValue(10);
        else setZValue(1);
    }
    if (change == ItemPositionChange && scene() && parentItem() == 0) {
        // calculate new position.
        const int trackHeight = KdenliveSettings::trackheight();
        QPointF start = sceneBoundingRect().topLeft();
        QPointF newPos = value.toPointF();
        int xpos = projectScene()->getSnapPointForPos((int)(start.x() + newPos.x() - pos().x()), KdenliveSettings::snaptopoints());

        xpos = qMax(xpos, 0);
        //kDebug()<<"GRP XPOS:"<<xpos<<", START:"<<start.x()<<",NEW:"<<newPos.x()<<"; SCENE:"<<scenePos().x()<<",POS:"<<pos().x();
        newPos.setX((int)(pos().x() + xpos - (int) start.x()));

	int yOffset = property("y_absolute").toInt() + newPos.y();
        int proposedTrack = yOffset / trackHeight;

        // Check if top item is a clip or a transition
        int offset = 0;
        int topTrack = -1;
        QList<QGraphicsItem *> children = childItems();
        for (int i = 0; i < children.count(); i++) {
            int currentTrack = 0;
	    if (children.at(i)->type() == AVWIDGET || children.at(i)->type() == TRANSITIONWIDGET) currentTrack = static_cast <AbstractClipItem*> (children.at(i))->track();
	    else if (children.at(i)->type() == GROUPWIDGET) currentTrack = static_cast <AbstractGroupItem*> (children.at(i))->track();
	    else continue;
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

        QList<QGraphicsItem*> collidingItems;
        QPainterPath shape;
        if (projectScene()->editMode() == NORMALEDIT) {
            shape = clipGroupShape(newPos - pos());
            collidingItems = scene()->items(shape, Qt::IntersectsItemShape);
            collidingItems.removeAll(this);
            for (int i = 0; i < children.count(); i++) {
                if (children.at(i)->type() == GROUPWIDGET) {
                    QList<QGraphicsItem *> subchildren = children.at(i)->childItems();
                    for (int j = 0; j < subchildren.count(); j++) {
                        collidingItems.removeAll(subchildren.at(j));
                    }
                }
                collidingItems.removeAll(children.at(i));
            }
        }
        if (!collidingItems.isEmpty()) {
            bool forwardMove = xpos > start.x();
            int offset = 0;
            for (int i = 0; i < collidingItems.count(); i++) {
                QGraphicsItem *collision = collidingItems.at(i);
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
                collidingItems = scene()->items(clipGroupShape(newPos - pos()), Qt::IntersectsItemShape);
                collidingItems.removeAll(this);
                for (int i = 0; i < children.count(); i++) {
                    if (children.at(i)->type() == GROUPWIDGET) {
                        QList<QGraphicsItem *> subchildren = children.at(i)->childItems();
                        for (int j = 0; j < subchildren.count(); j++) {
                            collidingItems.removeAll(subchildren.at(j));
                        }
                    }
                    collidingItems.removeAll(children.at(i));
                }
                for (int i = 0; i < collidingItems.count(); i++)
                    if (collidingItems.at(i)->type() == AVWIDGET) return pos();
            }
        }

        if (projectScene()->editMode() == NORMALEDIT) {
            shape = transitionGroupShape(newPos - pos());
            collidingItems = scene()->items(shape, Qt::IntersectsItemShape);
            collidingItems.removeAll(this);
            for (int i = 0; i < children.count(); i++) {
                if (children.at(i)->type() == GROUPWIDGET) {
                    QList<QGraphicsItem *> subchildren = children.at(i)->childItems();
                    for (int j = 0; j < subchildren.count(); j++) {
                        collidingItems.removeAll(subchildren.at(j));
                    }
                }
                collidingItems.removeAll(children.at(i));
            }
        }
        if (collidingItems.isEmpty()) return newPos;
        else {
            bool forwardMove = xpos > start.x();
            int offset = 0;
            for (int i = 0; i < collidingItems.count(); i++) {
                QGraphicsItem *collision = collidingItems.at(i);
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
                collidingItems = scene()->items(transitionGroupShape(newPos - pos()), Qt::IntersectsItemShape);
                for (int i = 0; i < children.count(); i++) {
                    collidingItems.removeAll(children.at(i));
                }
                for (int i = 0; i < collidingItems.count(); i++)
                    if (collidingItems.at(i)->type() == TRANSITIONWIDGET) return pos();
            }
        }
        return newPos;
    }
    return QGraphicsItemGroup::itemChange(change, value);
}

//virtual
void AbstractGroupItem::dropEvent(QGraphicsSceneDragDropEvent * event)
{
    QString effects = QString::fromUtf8(event->mimeData()->data("kdenlive/effectslist"));
    QDomDocument doc;
    doc.setContent(effects, true);
    QDomElement e = doc.documentElement();
    e.setAttribute("kdenlive_ix", 0);
    CustomTrackView *view = (CustomTrackView *) scene()->views()[0];
    QPointF dropPos = event->scenePos();
    QList<QGraphicsItem *> selection = scene()->items(dropPos);
    AbstractClipItem *dropChild = NULL;
    for (int i = 0; i < selection.count(); i++) {
	if (selection.at(i)->type() == AVWIDGET) {
            dropChild = (AbstractClipItem *) selection.at(i);
	    break;
        }
    }           
    if (view) view->slotAddGroupEffect(e, this, dropChild);
}

//virtual
void AbstractGroupItem::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    event->setAccepted(event->mimeData()->hasFormat("kdenlive/effectslist"));
}

void AbstractGroupItem::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
    Q_UNUSED(event)
}

// virtual
void AbstractGroupItem::mousePressEvent(QGraphicsSceneMouseEvent * event)
{
    if (event->modifiers() & Qt::ShiftModifier) {
        // User want to do a rectangle selection, so ignore the event to pass it to the view
        event->ignore();
    } else QGraphicsItem::mousePressEvent(event);
}

void AbstractGroupItem::resizeStart(int diff)
{
    bool info = false;
    if (m_resizeInfos.isEmpty())
        info = true;
    int maximum = diff;
    QList <QGraphicsItem *> children = childItems();
    QList <AbstractClipItem *> items;
    int itemcount = 0;
    for (int i = 0; i < children.count(); ++i) {
        AbstractClipItem *item = static_cast <AbstractClipItem *>(children.at(i));
        if (item && item->type() == AVWIDGET) {
            items << item;
            if (info)
                m_resizeInfos << item->info();
            item->resizeStart((int)(m_resizeInfos.at(itemcount).startPos.frames(item->fps())) + diff);
            int itemdiff = (int)(item->startPos() - m_resizeInfos.at(itemcount).startPos).frames(item->fps());
            if (qAbs(itemdiff) < qAbs(maximum))
                maximum = itemdiff;
            ++itemcount;
        }
    }
    
    for (int i = 0; i < items.count(); ++i)
        items.at(i)->resizeStart((int)(m_resizeInfos.at(i).startPos.frames(items.at(i)->fps())) + maximum);
}

void AbstractGroupItem::resizeEnd(int diff)
{
    bool info = false;
    if (m_resizeInfos.isEmpty())
        info = true;
    int maximum = diff;
    QList <QGraphicsItem *> children = childItems();
    QList <AbstractClipItem *> items;
    int itemcount = 0;
    for (int i = 0; i < children.count(); ++i) {
        AbstractClipItem *item = static_cast <AbstractClipItem *>(children.at(i));
        if (item && item->type() == AVWIDGET) {
            items << item;
            if (info)
                m_resizeInfos << item->info();
            item->resizeEnd((int)(m_resizeInfos.at(itemcount).endPos.frames(item->fps())) + diff);
            int itemdiff = (int)(item->endPos() - m_resizeInfos.at(itemcount).endPos).frames(item->fps());
            if (qAbs(itemdiff) < qAbs(maximum))
                maximum = itemdiff;
            ++itemcount;
        }
    }

    for (int i = 0; i < items.count(); ++i)
        items.at(i)->resizeEnd((int)(m_resizeInfos.at(i).endPos.frames(items.at(i)->fps())) + maximum);
}

QList< ItemInfo > AbstractGroupItem::resizeInfos()
{
    return m_resizeInfos;
}

void AbstractGroupItem::clearResizeInfos()
{
    // m_resizeInfos.clear() will crash in some cases for unknown reasons - ttill
    m_resizeInfos = QList <ItemInfo>();
}

GenTime AbstractGroupItem::duration()
{
    QList <QGraphicsItem *> children = childItems();
    GenTime start = GenTime(-1.0);
    GenTime end = GenTime();
    for (int i = 0; i < children.count(); ++i) {
        if (children.at(i)->type() != GROUPWIDGET) {
            AbstractClipItem *item = static_cast <AbstractClipItem *>(children.at(i));
            if (item) {
                if (start < GenTime() || item->startPos() < start)
                    start = item->startPos();
                if (item->endPos() > end)
                    end = item->endPos();
            }
        } else {
            children << children.at(i)->childItems();
        }
    }
    return end - start;
}
