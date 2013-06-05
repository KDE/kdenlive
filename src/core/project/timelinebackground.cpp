/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "timelinebackground.h"
#include "timeline.h"
#include "producerwrapper.h"


TimelineBackground::TimelineBackground(ProducerWrapper* track, Timeline* parent) :
    QObject(parent),
    m_parent(parent),
    m_playlist(new Mlt::Playlist((mlt_playlist)track->get_service()))
{
    connect(parent, SIGNAL(durationChanged(int)), this, SLOT(onTimelineDurationChange()));
}

TimelineBackground::~TimelineBackground()
{
    delete m_playlist;
}

void TimelineBackground::onTimelineDurationChange()
{
    int duration = m_playlist->get_playtime();

    // if the other tracks were shortened this track prevents us from seing the real timeline duration
    // so hide it to find out the real timeline duration
    m_parent->producer()->block();
    m_playlist->set_in_and_out(0, 0);
    int timelineDuration = m_parent->duration();
    m_playlist->set_in_and_out(0, duration - 1);
    m_parent->producer()->unblock();

    if (timelineDuration == duration) {
        return;
    }

    Mlt::Producer *clip = m_playlist->get_clip(0);
    if (timelineDuration > clip->get_length()) {
        clip->parent().set("length", timelineDuration);
        clip->parent().set("out", timelineDuration - 1);
        clip->set("length", timelineDuration);
    }
    m_playlist->resize_clip(0, 0, timelineDuration - 1);
    delete clip;
}

#include "timelinebackground.moc"
