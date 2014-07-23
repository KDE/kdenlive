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
#include "customtrackscene.h"
#include "customtrackview.h"

#include "kdenlivesettings.h"

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
    return GroupWidget;
}

int AbstractGroupItem::track() const
{
    //return (int)(scenePos().y() / KdenliveSettings::trackheight());
    int topTrack = -1;
    QList<QGraphicsItem *> children = childItems();
    for (int i = 0; i < children.count(); ++i) {
        if (children.at(i)->type() == GroupWidget) {
            children.append(children.at(i)->childItems());
            continue;
        }
        AbstractClipItem *item = static_cast <AbstractClipItem *>(children.at(i));
        if (item && (topTrack == -1 || topTrack > item->track())) {
            topTrack = item->track();
        }
    }
    return topTrack;
}

void AbstractGroupItem::setItemLocked(bool locked)
{
    if (locked)
        setSelected(false);

    setFlag(QGraphicsItem::ItemIsMovable, !locked);
    setFlag(QGraphicsItem::ItemIsSelectable, !locked);

    foreach (QGraphicsItem *child, childItems())
        static_cast<AbstractClipItem*>(child)->setItemLocked(locked);
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

QPainterPath AbstractGroupItem::clipGroupSpacerShape(const QPointF &offset) const
{
    return spacerGroupShape(AVWidget, offset);
}

QPainterPath AbstractGroupItem::clipGroupShape(const QPointF &offset) const
{
    return groupShape(AVWidget, offset);
}

QPainterPath AbstractGroupItem::transitionGroupShape(const QPointF &offset) const
{
    return groupShape(TransitionWidget, offset);
}

QPainterPath AbstractGroupItem::groupShape(GraphicsRectItem type, const QPointF &offset) const
{
    QPainterPath path;
    QList<QGraphicsItem *> children = childItems();
    for (int i = 0; i < children.count(); ++i) {
        if (children.at(i)->type() == (int)type) {
            QRectF r(children.at(i)->sceneBoundingRect());
            r.translate(offset);
            path.addRect(r);
        } else if (children.at(i)->type() == GroupWidget) {
            QList<QGraphicsItem *> subchildren = children.at(i)->childItems();
            for (int j = 0; j < subchildren.count(); ++j) {
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

QPainterPath AbstractGroupItem::spacerGroupShape(GraphicsRectItem type, const QPointF &offset) const
{
    QPainterPath path;
    QList<QGraphicsItem *> children = childItems();
    for (int i = 0; i < children.count(); ++i) {
        if (children.at(i)->type() == (int)type) {
            QRectF r(children.at(i)->sceneBoundingRect());
            r.translate(offset);
            r.setRight(scene()->width());
            path.addRect(r);
        } else if (children.at(i)->type() == GroupWidget) {
            QList<QGraphicsItem *> subchildren = children.at(i)->childItems();
            for (int j = 0; j < subchildren.count(); ++j) {
                if (subchildren.at(j)->type() == (int)type) {
                    QRectF r(subchildren.at(j)->sceneBoundingRect());
                    r.translate(offset);
                    r.setRight(scene()->width());
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

void AbstractGroupItem::removeItem(QGraphicsItem * item)
{
    removeFromGroup(item);
}

// virtual
void AbstractGroupItem::paint(QPainter *p, const QStyleOptionGraphicsItem *option, QWidget *)
{
    QColor bgcolor(100, 100, 200, 100);
    QRectF bound = option->exposedRect.adjusted(0, 0, 1, 1);
    p->setClipRect(bound);
    p->setBrush(bgcolor);
    QPen pen = p->pen();
    pen.setColor(QColor(200, 90, 90));
    pen.setStyle(Qt::DashLine);
    pen.setWidthF(0.0);
    p->setPen(pen);
    p->drawRoundedRect(boundingRect().adjusted(0, 0, -1, 0), 3, 3);
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
        //kDebug()<<"GRP XPOS:"<<xpos<<", START:"<<start.x()<<",NEW:"<<newPos.x()<<"; SCENE:"<<scenePos().x()<<",POS:"<<pos().x();
        newPos.setX((int)(pos().x() + xpos - (int) start.x()));
        QStringList lockedTracks = property("locked_tracks").toStringList();
        int proposedTrack = (property("y_absolute").toInt() + newPos.y()) / trackHeight;
        // Check if top item is a clip or a transition
        int offset = 0;
        int topTrack = -1;
        QList<int> groupTracks;
        QList<QGraphicsItem *> children = childItems();
        for (int i = 0; i < children.count(); ++i) {
            int currentTrack = 0;
            if (children.at(i)->type() == AVWidget || children.at(i)->type() == TransitionWidget) {
                currentTrack = static_cast <AbstractClipItem*> (children.at(i))->track();
                if (!groupTracks.contains(currentTrack)) groupTracks.append(currentTrack);
            }
            else if (children.at(i)->type() == GroupWidget) {
                currentTrack = static_cast <AbstractGroupItem*> (children.at(i))->track();
            }
            else continue;
            if (children.at(i)->type() == AVWidget) {
                if (topTrack == -1 || currentTrack <= topTrack) {
                    offset = 0;
                    topTrack = currentTrack;
                }
            } else if (children.at(i)->type() == TransitionWidget) {
                if (topTrack == -1 || currentTrack < topTrack) {
                    offset = (int)(trackHeight / 3 * 2 - 1);
                    topTrack = currentTrack;
                }
            } else if (children.at(i)->type() == GroupWidget) {
                QList<QGraphicsItem *> subchildren = children.at(i)->childItems();
                bool clipGroup = false;
                for (int j = 0; j < subchildren.count(); ++j) {
                    if (subchildren.at(j)->type() == AVWidget || subchildren.at(j)->type() == TransitionWidget) {
                        int subTrack = static_cast <AbstractClipItem*> (subchildren.at(j))->track();
                        if (!groupTracks.contains(subTrack)) groupTracks.append(subTrack);
                        clipGroup = true;
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
        // Check no clip in the group goes outside of existing tracks
        int maximumTrack = projectScene()->tracksCount() - 1;
        int groupHeight = 0;
        for (int i = 0; i < groupTracks.count(); ++i) {
            int offset = groupTracks.at(i) - topTrack;
            if (offset > groupHeight) groupHeight = offset;
        }
        maximumTrack -= groupHeight;
        proposedTrack = qMin(proposedTrack, maximumTrack);
        proposedTrack = qMax(proposedTrack, 0);
        int groupOffset = proposedTrack - topTrack;
        if (!lockedTracks.isEmpty()) {
            for (int i = 0; i < groupTracks.count(); ++i) {
                if (lockedTracks.contains(QString::number(groupTracks.at(i) + groupOffset))) {
                    return pos();
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
        if (projectScene()->editMode() == NormalEdit) {
            shape = clipGroupShape(newPos - pos());
            collidingItems = scene()->items(shape, Qt::IntersectsItemShape);
            collidingItems.removeAll(this);
            for (int i = 0; i < children.count(); ++i) {
                if (children.at(i)->type() == GroupWidget) {
                    QList<QGraphicsItem *> subchildren = children.at(i)->childItems();
                    for (int j = 0; j < subchildren.count(); ++j) {
                        collidingItems.removeAll(subchildren.at(j));
                    }
                }
                collidingItems.removeAll(children.at(i));
            }
        }
        if (!collidingItems.isEmpty()) {
            bool forwardMove = xpos > start.x();
            int offset = 0;
            for (int i = 0; i < collidingItems.count(); ++i) {
                QGraphicsItem *collision = collidingItems.at(i);
                if (collision->type() == AVWidget) {
                    // Collision
                    if (newPos.y() != pos().y()) {
                        // Track change results in collision, restore original position
                        return pos();
                    }
                    AbstractClipItem *item = static_cast <AbstractClipItem *>(collision);
                    // Determine best pos
                    QPainterPath clipPath;
                    clipPath.addRect(item->sceneBoundingRect());
                    QPainterPath res = shape.intersected(clipPath);
                    offset = qMax(offset, (int)(res.boundingRect().width() + 0.5));
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
                for (int i = 0; i < children.count(); ++i) {
                    if (children.at(i)->type() == GroupWidget) {
                        QList<QGraphicsItem *> subchildren = children.at(i)->childItems();
                        for (int j = 0; j < subchildren.count(); ++j) {
                            collidingItems.removeAll(subchildren.at(j));
                        }
                    }
                    collidingItems.removeAll(children.at(i));
                }
                for (int i = 0; i < collidingItems.count(); ++i)
                    if (collidingItems.at(i)->type() == AVWidget) return pos();
            }
        }

        if (projectScene()->editMode() == NormalEdit) {
            shape = transitionGroupShape(newPos - pos());
            collidingItems = scene()->items(shape, Qt::IntersectsItemShape);
            collidingItems.removeAll(this);
            for (int i = 0; i < children.count(); ++i) {
                if (children.at(i)->type() == GroupWidget) {
                    QList<QGraphicsItem *> subchildren = children.at(i)->childItems();
                    for (int j = 0; j < subchildren.count(); ++j) {
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
            for (int i = 0; i < collidingItems.count(); ++i) {
                QGraphicsItem *collision = collidingItems.at(i);
                if (collision->type() == TransitionWidget) {
                    // Collision
                    if (newPos.y() != pos().y()) {
                        // Track change results in collision, restore original position
                        return pos();
                    }
                    AbstractClipItem *item = static_cast <AbstractClipItem *>(collision);
                    // Determine best pos
		    QPainterPath clipPath;
		    clipPath.addRect(item->sceneBoundingRect());
		    QPainterPath res = shape.intersected(clipPath);
		    offset = qMax(offset, (int)(res.boundingRect().width() + 0.5));
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
                for (int i = 0; i < children.count(); ++i) {
                    collidingItems.removeAll(children.at(i));
                }
                for (int i = 0; i < collidingItems.count(); ++i)
                    if (collidingItems.at(i)->type() == TransitionWidget) return pos();
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
    CustomTrackView *view = static_cast<CustomTrackView*>(scene()->views()[0]);
    QPointF dropPos = event->scenePos();
    QList<QGraphicsItem *> selection = scene()->items(dropPos);
    AbstractClipItem *dropChild = NULL;
    for (int i = 0; i < selection.count(); ++i) {
        if (selection.at(i)->type() == AVWidget) {
            dropChild = static_cast<AbstractClipItem*>(selection.at(i));
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
    } else {
        QList <QGraphicsItem *>list = scene()->items(event->scenePos());
        // only allow group move if we click over an item in the group
        foreach(const QGraphicsItem *item, list) {
            if (item->type() == TransitionWidget || item->type() == AVWidget) {
                QGraphicsItem::mousePressEvent(event);
                return;
            }
        }
        event->ignore();
    }
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
        if (item && item->type() == AVWidget) {
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
        if (item && item->type() == AVWidget) {
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
        if (children.at(i)->type() != GroupWidget) {
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

#include "abstractgroupitem.moc"
