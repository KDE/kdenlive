/*
Copyright (C) 2013  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "setclippostionitemaction.h"
#include "core/timelineview/timelineclipitem.h"
#include "core/timelineview/timelinetrackitem.h"
#include "core/project/abstracttimelineclip.h"
#include "core/project/abstractprojectclip.h"
#include "core/project/timelinetrack.h"
#include <QGraphicsSceneMouseEvent>


SetClipPositionItemAction::SetClipPositionItemAction(TimelineClipItem* clipItem, QEvent* initialEvent, QObject* parent) :
    AbstractClipItemAction(clipItem, initialEvent, parent)
{
    slotEvent(initialEvent);
}

SetClipPositionItemAction::~SetClipPositionItemAction()
{
}

void SetClipPositionItemAction::mousePress(QGraphicsSceneMouseEvent* event)
{
    Q_UNUSED(event)

    m_original = qRound(event->scenePos().x());
}

void SetClipPositionItemAction::mouseMove(QGraphicsSceneMouseEvent* event)
{
    int boundLeft = 0, boundRight = 0;
    int lastPosition = m_clip->rect().x();

    QGraphicsItem * lastTrack = m_clip->parentItem();
    TimelineTrackItem *newTrack = static_cast<TimelineScene*>(m_clip->scene())->trackItemAt(event->scenePos().y());
    if (newTrack) {
        m_clip->setParentItem(newTrack);
    }
    
    m_clip->setGeometry(qMax(0, qRound(event->scenePos().x()) - m_original + m_clip->clip()->position()), m_clip->rect().width());

    QList<QGraphicsItem *> collisions = m_clip->collidingItems();
    foreach (QGraphicsItem * const &item, collisions) {
        if (item->type() == TimelineClipItem::Type
            && item->parentItem() == m_clip->parentItem()
            && item != m_clip) {
            TimelineClipItem *clip = static_cast<TimelineClipItem *>(item);
            if (clip->rect().x() < m_clip->rect().x()) {
                boundLeft = clip->clip()->position() + clip->clip()->duration();
            } else {
                boundRight = clip->clip()->position() - m_clip->clip()->duration();
            }
        }
    }
    
    int position = qMax(boundLeft, qRound(m_clip->rect().x()));
    if (boundRight > boundLeft) {
        position = qMin(boundRight, position);
    }
    
    m_clip->setGeometry(position, m_clip->rect().width());

    // make sure we did not end up in a gap too small
    collisions = m_clip->collidingItems();
    foreach (QGraphicsItem * const &item, collisions) {
        if (item->type() == TimelineClipItem::Type
            && item->parentItem() == m_clip->parentItem()
            && item != m_clip
            && m_clip->rect().intersected(item->boundingRect()).width() > 1) {
            m_clip->setParentItem(lastTrack);
            m_clip->setGeometry(lastPosition, m_clip->rect().width());
            break;
        }
    }
}

void SetClipPositionItemAction::mouseRelease(QGraphicsSceneMouseEvent* event)
{
    Q_UNUSED(event)

    m_clip->clip()->setPosition(m_clip->rect().x(), static_cast<TimelineTrackItem*>(m_clip->parentItem())->track()->index());

    emit finished();
}

#include "setclippostionitemaction.moc"

