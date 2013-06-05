/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "timelinescene.h"
#include "positionitem.h"
#include "timelinetrackitem.h"
#include "tool/toolmanager.h"
#include "project/timeline.h"
#include "project/timelinetrack.h"
#include <QGraphicsSceneMouseEvent>
#include <QMimeData>
#include <KDebug>


TimelineScene::TimelineScene(Timeline* timeline, ToolManager *toolManager, TimelineView *view, QObject* parent) :
    QGraphicsScene(parent),
    m_timeline(timeline),
    m_view(view),
    m_toolManager(toolManager)
{
    setupTimeline();

    addItem(new PositionItem(this));
}

TimelineScene::~TimelineScene()
{
}

Timeline* TimelineScene::timeline()
{
    return m_timeline;
}

TimelineView* TimelineScene::view()
{
    return m_view;
}

TimelineTrackItem* TimelineScene::trackItem(int index)
{
    return m_trackItems.at(index);
}

TimelineTrackItem* TimelineScene::trackItemAt(int yPos)
{
    QList<QGraphicsItem*> itemsAtPos = items(0, yPos, 1, 1, Qt::IntersectsItemShape, Qt::DescendingOrder);
    foreach (QGraphicsItem * const &item, itemsAtPos) {
        if (item->type() == TimelineTrackItem::Type) {
            return qgraphicsitem_cast<TimelineTrackItem*>(item);
        }
    }

    return 0;
}

ToolManager* TimelineScene::toolManager()
{
    return m_toolManager;
}

void TimelineScene::positionTracks(TimelineTrackItem *after)
{
    int i = 0;
    qreal position = 0;

    if (after) {
        i = m_trackItems.indexOf(after) + 1;
        position = after->rect().bottom();
    }

    for (; i < m_trackItems.count(); ++i) {
        m_trackItems.at(i)->setY(position);
        position += m_trackItems.at(i)->rect().height();
    }

    emit heightChanged(position);
}

void TimelineScene::setupTimeline()
{
    QList<TimelineTrack*> tracks = m_timeline->tracks();
    foreach (TimelineTrack *track, tracks) {
        TimelineTrackItem *trackItem = new TimelineTrackItem(track, this);
        addItem(trackItem);
        m_trackItems.append(trackItem);
    }
    positionTracks();
}

void TimelineScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsScene::mouseMoveEvent(event);
    if (!event->isAccepted()) {
        m_toolManager->sceneEvent(this, event);
    }
}

void TimelineScene::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsScene::mousePressEvent(event);
    if (!event->isAccepted()) {
        m_toolManager->sceneEvent(this, event);
    }
}

void TimelineScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    QGraphicsScene::mousePressEvent(event);
    if (!event->isAccepted()) {
        m_toolManager->sceneEvent(this, event);
    }
}

void TimelineScene::dragEnterEvent(QGraphicsSceneDragDropEvent* event)
{
    event->setAccepted(event->mimeData()->hasFormat("kdenlive/clip"));
}

void TimelineScene::dragMoveEvent(QGraphicsSceneDragDropEvent* event)
{
    kDebug()<<" + + + ++ + LOL POP EVENT";
    m_toolManager->sceneEvent(this, event);
}

void TimelineScene::dragLeaveEvent(QGraphicsSceneDragDropEvent* event)
{

}

void TimelineScene::dropEvent(QGraphicsSceneDragDropEvent* event)
{
    
}

#include "timelinescene.moc"
