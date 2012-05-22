/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "abstracttimelineclip.h"
#include "producerwrapper.h"
#include "abstractprojectclip.h"
#include "timelinetrack.h"

AbstractTimelineClip::AbstractTimelineClip(ProducerWrapper* producer, AbstractProjectClip* projectClip, TimelineTrack* parent) :
    m_producer(producer),
    m_projectClip(projectClip),
    m_parent(parent)
{
    
}

AbstractTimelineClip::~AbstractTimelineClip()
{
    if (m_producer) {
        delete m_producer;
    }
}

ProducerWrapper* AbstractTimelineClip::producer()
{
    return m_producer;
}

AbstractProjectClip* AbstractTimelineClip::projectClip()
{
    return m_projectClip;
}

TimelineTrack* AbstractTimelineClip::track()
{
    return m_parent;
}

#include "abstracttimelineclip.moc"
