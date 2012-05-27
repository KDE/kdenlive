/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "timelinetrackitem.h"
#include "timelinescene.h"
#include "timelineclipitem.h"
#include "project/timelinetrack.h"
#include "project/abstracttimelineclip.h"


TimelineTrackItem::TimelineTrackItem(TimelineTrack* track, QObject* parent) :
    QObject(parent),
    m_track(track)
{
    setRect(0, 0, 0, 50);
    setBrush(Qt::red);

    loadClips();
    adjustLength();
}

TimelineTrackItem::~TimelineTrackItem()
{

}

TimelineTrack* TimelineTrackItem::track()
{
    return m_track;
}

void TimelineTrackItem::adjustLength()
{
    QRectF r = rect();
    if (m_clipItems.isEmpty()) {
        r.setWidth(0);
    } else {
        r.setWidth(m_clipItems.last()->rect().right());
    }
    setRect(r);
}

void TimelineTrackItem::loadClips()
{
    QList <AbstractTimelineClip*> clips = m_track->clips();
    foreach(AbstractTimelineClip *clip, clips) {
        TimelineClipItem *item = new TimelineClipItem(clip, this);
        m_clipItems.append(item);
    }
}


#include "timelinetrackitem.moc"
