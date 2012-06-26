/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "timeline.h"
#include "project.h"
#include "producerwrapper.h"
#include "timelinetrack.h"
#include "timelinebackground.h"
#include "monitor/monitormodel.h"
#include "kdenlivesettings.h"
#include <mlt++/Mlt.h>
#include <KLocale>


Timeline::Timeline(const QString& document, Project* parent) :
    QObject(parent),
    m_parent(parent)
{
    // profile we set doesn't matter, it will be overwritten anyways with the info in the profile tag
    m_profile = new Mlt::Profile(KdenliveSettings::default_profile().toUtf8().constData());
    m_producer = new ProducerWrapper(*m_profile, document, "xml-string");

    Q_ASSERT(m_producer && m_producer->is_valid());

    Mlt::Service service(m_producer->parent().get_service());

    Q_ASSERT(service.type() == tractor_type);

    m_tractor = new Mlt::Tractor(service);

    m_monitor = new MonitorModel(m_profile, i18n("Timeline"), this);
    m_monitor->setProducer(m_producer);
//     m_producer->optimise();

    m_producerChangeEvent = m_producer->listen("producer-changed", this, (mlt_listener)producer_change);
}

Timeline::Timeline(Project* parent) :
    QObject(parent),
    m_parent(parent),
    m_producerChangeEvent(0)
{
    m_profile = new Mlt::Profile(KdenliveSettings::default_profile().toUtf8().constData());
    m_tractor = new Mlt::Tractor();
    m_producer = 0;

//     m_producer = m_tractor->parent().producer();

//     Q_ASSERT(m_producer && m_producer->is_valid());
}

Timeline::~Timeline()
{
    qDeleteAll(m_tracks);
    delete m_producerChangeEvent;
    delete m_producer;
    delete m_profile;
}

int Timeline::duration() const
{
    return m_tractor->get_playtime();
}

Project* Timeline::project()
{
    return m_parent;
}

QList< TimelineTrack* > Timeline::tracks()
{
    return m_tracks;
}

Mlt::Profile* Timeline::profile()
{
    return m_profile;
}

ProducerWrapper* Timeline::producer()
{
    return m_producer;
}

MonitorModel* Timeline::monitor()
{
    return m_monitor;
//     return m_parent->binMonitor();
}

TimelineTrack* Timeline::track(int index)
{
    return m_tracks.at(index);
}

void Timeline::loadTracks()
{
    Q_ASSERT(m_tracks.count() == 0);

    Mlt::Multitrack *multitrack = m_tractor->multitrack();
    int min = 0;
    if (multitrack->track(0)->get("id") == QString("black_track")) {
        new TimelineBackground(new ProducerWrapper(multitrack->track(0)), this);
        ++min;
    }
    for (int i = multitrack->count() - 1; i >= min; --i) {
        TimelineTrack *track = new TimelineTrack(new ProducerWrapper(multitrack->track(i)), this);
        if (track) {
            m_tracks.append(track);
        }
    }
}

void Timeline::emitDurationChanged()
{
    emit durationChanged(duration());
}

void Timeline::producer_change(mlt_producer producer, Timeline* self)
{
    self->emitDurationChanged();
}

#include "timeline.moc"
