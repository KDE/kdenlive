/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "timelinetrack.h"
#include "timeline.h"
#include "producerwrapper.h"
#include "abstracttimelineclip.h"
#include "project.h"
#include "abstractprojectclip.h"
#include "binmodel.h"
#include "effectsystem/effectdevice.h"

#include <KDebug>


TimelineTrack::TimelineTrack(ProducerWrapper* producer, Timeline* parent) :
    QObject(parent),
    m_parent(parent),
    m_producer(producer),
    m_playlist(new Mlt::Playlist((mlt_playlist)producer->get_service()))
{
    for (int i = 0; i < m_playlist->count(); ++i) {
        ProducerWrapper *clipProducer = new ProducerWrapper(m_playlist->get_clip(i));
        if (!clipProducer->is_blank()) {
            QString id = QString(clipProducer->parent().get("id")).section('_', 0, 0);
            AbstractProjectClip *projectClip = parent->project()->bin()->clip(id.toInt());
            if (projectClip) {
                kDebug() << "project clip found" << id;
                AbstractTimelineClip *clip = projectClip->addInstance(clipProducer, this);
                m_clips.insert(i, clip);
            } else {
                kDebug() << "project clip not found" << id;
                delete clipProducer;
            }
        } else {
            delete clipProducer;
        }
    }
    kDebug() << "track created" << producer->get("name") << m_clips.count() << m_playlist->count();
}

TimelineTrack::~TimelineTrack()
{
    qDeleteAll(m_clips);
    delete m_producer;
}

Timeline* TimelineTrack::timeline()
{
    return m_parent;
}

ProducerWrapper* TimelineTrack::producer()
{
    return m_producer;
}

Mlt::Playlist* TimelineTrack::playlist()
{
    return m_playlist;
}

QList< AbstractTimelineClip* > TimelineTrack::clips()
{
    return m_clips.values();
}

int TimelineTrack::clipPosition(const AbstractTimelineClip* clip) const
{
    return m_playlist->clip(mlt_whence_relative_start, m_clips.key(const_cast<AbstractTimelineClip*>(clip)));
}

QString TimelineTrack::name() const
{
    return QString();
}

void TimelineTrack::setName(const QString& name)
{

}

#include "timelinetrack.moc"
