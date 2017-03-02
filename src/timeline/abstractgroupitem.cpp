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

#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QDomDocument>
#include <QGraphicsSceneMouseEvent>
#include <QMimeData>

AbstractGroupItem::AbstractGroupItem(double /* fps */) :
    QObject(),
    QGraphicsItemGroup()
{
    setZValue(1);
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);
    setAcceptDrops(true);
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
        if (item && (topTrack == -1 || topTrack < item->track())) {
            topTrack = item->track();
        }
    }
    return topTrack;
}

void AbstractGroupItem::setItemLocked(bool locked)
{
    if (locked) {
        setSelected(false);
    }

    setFlag(QGraphicsItem::ItemIsMovable, !locked);
    setFlag(QGraphicsItem::ItemIsSelectable, !locked);

    foreach (QGraphicsItem *child, childItems()) {
        static_cast<AbstractClipItem *>(child)->setItemLocked(locked);
    }
}

bool AbstractGroupItem::isItemLocked() const
{
    return !(flags() & (QGraphicsItem::ItemIsSelectable));
}

CustomTrackScene *AbstractGroupItem::projectScene()
{
    if (scene()) {
        return static_cast <CustomTrackScene *>(scene());
    }
    return nullptr;
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

void AbstractGroupItem::addItem(QGraphicsItem *item)
{
    addToGroup(item);
    item->setFlag(QGraphicsItem::ItemIsMovable, false);
}

void AbstractGroupItem::removeItem(QGraphicsItem *item)
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
    QRectF bd = childrenBoundingRect().adjusted(0, 0, 0, 0);
    p->drawRect(bd);
}

int AbstractGroupItem::trackForPos(int position)
{
    int track = 1;
    if (!scene() || scene()->views().isEmpty()) {
        return track;
    }
    CustomTrackView *view = static_cast<CustomTrackView *>(scene()->views()[0]);
    if (view) {
        track = view->getTrackFromPos(position);
    }
    return track;
}

int AbstractGroupItem::posForTrack(int track)
{
    int pos = 0;
    if (!scene() || scene()->views().isEmpty()) {
        return pos;
    }
    CustomTrackView *view = static_cast<CustomTrackView *>(scene()->views()[0]);
    if (view) {
        pos = view->getPositionFromTrack(track);
    }
    return pos;
}

//virtual
QVariant AbstractGroupItem::itemChange(GraphicsItemChange change, const QVariant &value)
{
    if (change == QGraphicsItem::ItemSelectedChange) {
        if (value.toBool()) {
            setZValue(3);
        } else {
            setZValue(1);
        }
    }
    CustomTrackScene *scene = nullptr;
    if (change == ItemPositionChange && parentItem() == nullptr) {
        scene = projectScene();
    }
    if (scene) {
        // calculate new position.
        if (scene->isZooming) {
            // For some reason, mouse wheel on selected itm sometimes triggered
            // a position change event corrupting timeline, so discard it
            return pos();
        }
        // calculate new position.
        const int trackHeight = KdenliveSettings::trackheight();
        QPointF start = sceneBoundingRect().topLeft();
        QPointF newPos = value.toPointF();
        int xpos = projectScene()->getSnapPointForPos((int)(start.x() + newPos.x() - pos().x()), KdenliveSettings::snaptopoints());

        xpos = qMax(xpos, 0);
        newPos.setX((int)(pos().x() + xpos - (int) start.x()));
        QList<int> lockedTracks = property("locked_tracks").value< QList<int> >();
        int proposedTrack = trackForPos(property("y_absolute").toInt() + newPos.y());
        // Check if top item is a clip or a transition
        int offset = 0;
        int topTrack = -1;
        QList<int> groupTracks;
        QList<QGraphicsItem *> children = childItems();
        for (int i = 0; i < children.count(); ++i) {
            int currentTrack = 0;
            if (children.at(i)->type() == AVWidget || children.at(i)->type() == TransitionWidget) {
                currentTrack = static_cast <AbstractClipItem *>(children.at(i))->track();
                if (!groupTracks.contains(currentTrack)) {
                    groupTracks.append(currentTrack);
                }
            } else if (children.at(i)->type() == GroupWidget) {
                currentTrack = static_cast <AbstractGroupItem *>(children.at(i))->track();
            } else {
                continue;
            }
            if (children.at(i)->type() == AVWidget) {
                if (topTrack == -1 || currentTrack >= topTrack) {
                    offset = 0;
                    topTrack = currentTrack;
                }
            } else if (children.at(i)->type() == TransitionWidget) {
                if (topTrack == -1 || currentTrack > topTrack) {
                    offset = (int)(trackHeight / 3 * 2 - 1);
                    topTrack = currentTrack;
                }
            } else if (children.at(i)->type() == GroupWidget) {
                QList<QGraphicsItem *> subchildren = children.at(i)->childItems();
                bool clipGroup = false;
                for (int j = 0; j < subchildren.count(); ++j) {
                    if (subchildren.at(j)->type() == AVWidget || subchildren.at(j)->type() == TransitionWidget) {
                        int subTrack = static_cast <AbstractClipItem *>(subchildren.at(j))->track();
                        if (!groupTracks.contains(subTrack)) {
                            groupTracks.append(subTrack);
                        }
                        clipGroup = true;
                    }
                }
                if (clipGroup) {
                    if (topTrack == -1 || currentTrack >= topTrack) {
                        offset = 0;
                        topTrack = currentTrack;
                    }
                } else {
                    if (topTrack == -1 || currentTrack > topTrack) {
                        offset = (int)(trackHeight / 3 * 2 - 1);
                        topTrack = currentTrack;
                    }
                }
            }
        }
        // Check no clip in the group goes outside of existing tracks
        int maximumTrack = projectScene()->tracksCount();
        int groupHeight = 0;
        for (int i = 0; i < groupTracks.count(); ++i) {
            int trackOffset = topTrack - groupTracks.at(i) + 1;
            if (trackOffset > groupHeight) {
                groupHeight = trackOffset;
            }
        }
        proposedTrack = qBound(groupHeight, proposedTrack, maximumTrack);
        int groupOffset = proposedTrack - topTrack;
        bool lockAdjust = false;
        if (!lockedTracks.isEmpty()) {
            for (int i = 0; i < groupTracks.count(); ++i) {
                if (lockedTracks.contains(groupTracks.at(i) + groupOffset)) {
                    newPos.setY(pos().y());
                    lockAdjust = true;
                    break;
                }
            }
        }
        if (!lockAdjust) {
            newPos.setY(posForTrack(proposedTrack) + offset);
        }
        //if (newPos == start) return start;

        /*if (newPos.x() < 0) {
            // If group goes below 0, adjust position to 0
            return QPointF(pos().x() - start.x(), pos().y());
        }*/

        QList<QGraphicsItem *> collidingItems;
        QPainterPath shape;
        if (projectScene()->editMode() == TimelineMode::NormalEdit) {
            shape = clipGroupShape(newPos - pos());
            collidingItems = scene->items(shape, Qt::IntersectsItemShape);
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
            int cur_offset = 0;
            for (int i = 0; i < collidingItems.count(); ++i) {
                QGraphicsItem *collision = collidingItems.at(i);
                if (collision->type() == AVWidget) {
                    // Collision
                    if (newPos.y() != pos().y()) {
                        // Track change results in collision, restore original position
                        newPos.setY(pos().y());
                    } else {
                        AbstractClipItem *item = static_cast <AbstractClipItem *>(collision);
                        // Determine best pos
                        QPainterPath clipPath;
                        clipPath.addRect(item->sceneBoundingRect());
                        QPainterPath res = shape.intersected(clipPath);
                        cur_offset = qMax(cur_offset, (int)(res.boundingRect().width() + 0.5));
                    }
                    break;
                }
            }
            if (forwardMove) {
                newPos.setX(newPos.x() - cur_offset);
            } else {
                newPos.setX(newPos.x() + cur_offset);
            }
            // If there is still a collision after our position adjust, restore original pos
            collidingItems = scene->items(clipGroupShape(newPos - pos()), Qt::IntersectsItemShape);
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
                if (collidingItems.at(i)->type() == AVWidget) {
                    return pos();
                }
        }

        if (projectScene()->editMode() == TimelineMode::NormalEdit) {
            shape = transitionGroupShape(newPos - pos());
            collidingItems = scene->items(shape, Qt::IntersectsItemShape);
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
        if (collidingItems.isEmpty()) {
            return newPos;
        } else {
            bool forwardMove = xpos > start.x();
            int cur_offset = 0;
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
                    cur_offset = qMax(cur_offset, (int)(res.boundingRect().width() + 0.5));
                }
            }
            if (cur_offset > 0) {
                if (forwardMove) {
                    newPos.setX(newPos.x() - cur_offset);
                } else {
                    newPos.setX(newPos.x() + cur_offset);
                }
                // If there is still a collision after our position adjust, restore original pos
                collidingItems = scene->items(transitionGroupShape(newPos - pos()), Qt::IntersectsItemShape);
                for (int i = 0; i < children.count(); ++i) {
                    collidingItems.removeAll(children.at(i));
                }
                for (int i = 0; i < collidingItems.count(); ++i)
                    if (collidingItems.at(i)->type() == TransitionWidget) {
                        return pos();
                    }
            }
        }
        return newPos;
    }
    return QGraphicsItemGroup::itemChange(change, value);
}

//virtual
void AbstractGroupItem::dropEvent(QGraphicsSceneDragDropEvent *event)
{
    QString effects = QString::fromUtf8(event->mimeData()->data(QStringLiteral("kdenlive/effectslist")));
    QDomDocument doc;
    doc.setContent(effects, true);
    QDomElement e = doc.documentElement();
    e.setAttribute(QStringLiteral("kdenlive_ix"), 0);
    CustomTrackView *view = static_cast<CustomTrackView *>(scene()->views()[0]);
    QPointF dropPos = event->scenePos();
    QList<QGraphicsItem *> selection = scene()->items(dropPos);
    AbstractClipItem *dropChild = nullptr;
    for (int i = 0; i < selection.count(); ++i) {
        if (selection.at(i)->type() == AVWidget) {
            dropChild = static_cast<AbstractClipItem *>(selection.at(i));
            break;
        }
    }
    if (view) {
        view->slotAddGroupEffect(e, this, dropChild);
    }
}

//virtual
void AbstractGroupItem::dragEnterEvent(QGraphicsSceneDragDropEvent *event)
{
    event->setAccepted(event->mimeData()->hasFormat(QStringLiteral("kdenlive/effectslist")));
}

void AbstractGroupItem::dragLeaveEvent(QGraphicsSceneDragDropEvent *event)
{
    Q_UNUSED(event)
}

// virtual
void AbstractGroupItem::mousePressEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->modifiers() & Qt::ShiftModifier) {
        // User want to do a rectangle selection, so ignore the event to pass it to the view
        event->ignore();
    } else {
        QList<QGraphicsItem *>list = scene()->items(event->scenePos());
        // only allow group move if we click over an item in the group
        foreach (const QGraphicsItem *item, list) {
            if (item->type() == TransitionWidget || item->type() == AVWidget) {
                QGraphicsItem::mousePressEvent(event);
                return;
            }
        }
        event->ignore();
    }
}

// virtual
void AbstractGroupItem::mouseReleaseEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        // User want to do a rectangle selection, so ignore the event to pass it to the view
        event->ignore();
    } else {
        QGraphicsItem::mouseReleaseEvent(event);
    }
}

void AbstractGroupItem::mouseMoveEvent(QGraphicsSceneMouseEvent *event)
{
    if (event->buttons() !=  Qt::LeftButton || event->modifiers() & Qt::ControlModifier) {
        // User want to do a rectangle selection, so ignore the event to pass it to the view
        event->ignore();
    } else {
        QGraphicsItem::mouseMoveEvent(event);
    }
}

void AbstractGroupItem::resizeStart(int diff)
{
    QList<QGraphicsItem *> children = childItems();
    for (int i = 0; i < children.count(); ++i) {
        AbstractClipItem *item = static_cast <AbstractClipItem *>(children.at(i));
        if (item && item->type() == AVWidget) {
            item->resizeStart(diff);
        }
    }
    update();
}

void AbstractGroupItem::resizeEnd(int diff)
{
    QList<QGraphicsItem *> children = childItems();
    for (int i = 0; i < children.count(); ++i) {
        AbstractClipItem *item = static_cast <AbstractClipItem *>(children.at(i));
        if (item && item->type() == AVWidget) {
            item->resizeEnd(diff);
        }
    }
    update();
}

GenTime AbstractGroupItem::duration() const
{
    QList<QGraphicsItem *> children = childItems();
    GenTime start = GenTime(-1.0);
    GenTime end = GenTime();
    for (int i = 0; i < children.count(); ++i) {
        if (children.at(i)->type() != GroupWidget) {
            AbstractClipItem *item = static_cast <AbstractClipItem *>(children.at(i));
            if (item) {
                if (start < GenTime() || item->startPos() < start) {
                    start = item->startPos();
                }
                if (item->endPos() > end) {
                    end = item->endPos();
                }
            }
        } else {
            children << children.at(i)->childItems();
        }
    }
    return end - start;
}

GenTime AbstractGroupItem::startPos() const
{
    QList<QGraphicsItem *> children = childItems();
    GenTime start = GenTime(-1.0);
    for (int i = 0; i < children.count(); ++i) {
        if (children.at(i)->type() != GroupWidget) {
            AbstractClipItem *item = static_cast <AbstractClipItem *>(children.at(i));
            if (item) {
                if (start < GenTime() || item->startPos() < start) {
                    start = item->startPos();
                }
            }
        } else {
            children << children.at(i)->childItems();
        }
    }
    return start;
}

QGraphicsItem *AbstractGroupItem::otherClip(QGraphicsItem *item)
{
    QList<QGraphicsItem *> children = childItems();
    if (children.isEmpty() || children.count() != 2) {
        return nullptr;
    }
    if (children.at(0) == item) {
        return children.at(1);
    } else {
        return children.at(0);
    }
}

QList<AbstractClipItem *> AbstractGroupItem::childClips() const
{
    QList<AbstractClipItem *> childClips;
    QList<QGraphicsItem *>children = childItems();
    for (int i = 0; i < children.count(); ++i) {
        if (children.at(i)->type() != GroupWidget) {
            AbstractClipItem *item = static_cast <AbstractClipItem *>(children.at(i));
            childClips << item;
        } else {
            AbstractGroupItem *grp = static_cast <AbstractGroupItem *>(children.at(i));
            childClips << grp->childClips();
        }
    }
    return childClips;
}
