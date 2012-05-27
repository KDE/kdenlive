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
#include "kdenlivesettings.h"
#include <mlt++/Mlt.h>


Timeline::Timeline(const QString& document, Project* parent) :
    QObject(parent),
    m_parent(parent)
{
    Mlt::Profile profile(KdenliveSettings::current_profile().toUtf8().constData());
    m_producer = new ProducerWrapper(profile, document.toUtf8().constData(), "xml-string");

    Q_ASSERT(m_producer && m_producer->is_valid());

    Mlt::Service service(m_producer->parent().get_service());

    Q_ASSERT(service.type() == tractor_type);

    m_tractor = new Mlt::Tractor(service);

//     m_producer->optimise();

    Mlt::Multitrack *multitrack = m_tractor->multitrack();
    for (int i = 0; i < multitrack->count(); ++i) {
        TimelineTrack *track = new TimelineTrack(new ProducerWrapper(multitrack->track(i)), this);
        if (track) {
            m_tracks.append(track);
        }
    }


    kDebug() << "timline created" << m_tracks.count();
}

Timeline::~Timeline()
{
    qDeleteAll(m_tracks);
    if (m_producer) {
        kDebug() << "deleteing m_tractorProducer";
        delete m_producer;
    }
}

Project* Timeline::project()
{
    return m_parent;
}

QList< TimelineTrack* > Timeline::tracks()
{
    return m_tracks;
}

#include "timeline.moc"
