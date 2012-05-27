/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "timelinescene.h"
#include "timelinetrackitem.h"
#include "project/timeline.h"
#include "project/timelinetrack.h"


TimelineScene::TimelineScene(Timeline* timeline, QObject* parent) :
    QGraphicsScene(parent),
    m_timeline(timeline)
{
    setupTimeline();
    positionTracks();
}

TimelineScene::~TimelineScene()
{
}

Timeline* TimelineScene::timeline()
{
    return m_timeline;
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
}

void TimelineScene::setupTimeline()
{
    QList<TimelineTrack*> tracks = m_timeline->tracks();
    foreach (TimelineTrack *track, tracks) {
        TimelineTrackItem *trackItem = new TimelineTrackItem(track, this);
        addItem(trackItem);
        m_trackItems.append(trackItem);
    }
}


#include "timelinescene.moc"
