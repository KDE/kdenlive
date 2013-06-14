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
#include <src/monitor.h>
#include "monitor/monitorview.h"
#include "kdenlivesettings.h"
#include <mlt++/Mlt.h>
#include <KLocale>
#include <KDebug>


Timeline::Timeline(const QString& document, Project* parent) :
    QObject(parent)
    , m_parent(parent)
    , m_monitor(NULL)
    , m_clipBinProducer(NULL)
{
    // profile we set doesn't matter, it will be overwritten anyways with the info in the profile tag
    Mlt::Profile *profile = new Mlt::Profile(KdenliveSettings::default_profile().toUtf8().constData());
    profile->set_explicit(0);
    kDebug()<<"// SETTING PROJECT PROFILE: "<<KdenliveSettings::default_profile();
    if (document.isEmpty()) {
        // Creating blank project
        QString blankDocument = getBlankDocument(profile);
        m_producer = new ProducerWrapper(*profile, blankDocument, "xml-string");

    } else {
        m_producer = new ProducerWrapper(*profile, document, "xml-string");
    }

    // this shouldn't be an assert
    Q_ASSERT(m_producer && m_producer->is_valid());

    Mlt::Service service(m_producer->parent().get_service());

    Q_ASSERT(service.type() == tractor_type);

    m_tractor = new Mlt::Tractor(service);

    //kDebug()<<"// loading prod: \n"<<document<<"\n-------------------";
    //TODO: create mon
    //m_monitor = new MonitorModel(m_profile, i18n("Timeline"), this);
    //m_monitor->setProducer(m_producer);
    
    //     m_producer->optimise();

    m_producerChangeEvent = m_producer->listen("producer-changed", this, (mlt_listener)producer_change);
}

Timeline::Timeline(ProducerWrapper *producer, Project* parent) :
    QObject(parent)
    , m_parent(parent)
    , m_monitor(NULL)
    , m_tractor(NULL)
    , m_clipBinProducer(producer)
{
    // profile we set doesn't matter, it will be overwritten anyways with the info in the profile tag
    //m_producer = producer;
    Mlt::Tractor *tractor = new Mlt::Tractor();
    Mlt::Playlist *playlist1 = new Mlt::Playlist(*producer->profile());
    playlist1->insert_at(0, producer, 1);
    //Mlt::Playlist *playlist2 = new Mlt::Playlist(*producer->profile());
    //Mlt::Playlist playlist2(*producer->profile());
    
    //TODO: proper number of tracks, etc...
    tractor->set_track(*playlist1, 0);
    //tractor->set_track(*playlist2, 1);
    
    Mlt::Producer *prod = new Mlt::Producer(tractor->get_producer());
    m_producer = new ProducerWrapper(prod);

    // this shouldn't be an assert
    Q_ASSERT(m_producer && m_producer->is_valid());
    m_producer->seek(producer->position());
    
    Mlt::Properties clipData(m_producer->get_properties());
    clipData.pass_list(*m_clipBinProducer, "kdenlive.timelineview_zoom");
    
    Mlt::Service service(m_producer->parent().get_service());

    Q_ASSERT(service.type() == tractor_type);

    m_tractor = new Mlt::Tractor(service);
    m_monitor = m_parent->binMonitor();

    m_producerChangeEvent = m_producer->listen("producer-changed", this, (mlt_listener)producer_change);
}

Timeline::~Timeline()
{
    if (m_clipBinProducer) {
	Mlt::Properties clipData(m_clipBinProducer->get_properties());
        clipData.pass_list(*m_producer, "kdenlive.timelineview_zoom");
	/*int count = clipData.count();
        for (int i = 0; i < count; i ++) {
            QString name = clipData.get_name(i);
            QString value = QString::fromUtf8(clipData.get(i));
	    kDebug()<<"/ / /PASSING VALUES : "<<name<<" = "<<value;
	}*/
	
    }
    qDeleteAll(m_tracks);
    delete m_producerChangeEvent;
    delete m_producer;
}

void Timeline::setMonitor(MonitorView *monitor)
{
    m_monitor = monitor;
    m_monitor->open(m_producer, ProjectMonitor);
}

QString Timeline::getBlankDocument(Mlt::Profile *profile) const
{
    // Generate an empty project xml
    QString playlist;
    Mlt::Consumer xmlConsumer(*profile, "xml:kdenlive_playlist");
    if (!xmlConsumer.is_valid()) return QString();
    Mlt::Tractor tractor;
    Mlt::Playlist playlist1(*profile);
    Mlt::Playlist playlist2(*profile);
    Mlt::Playlist playlist3(*profile);
    playlist1.set("id", "black_track");
    Mlt::Producer prod1(*profile, "color:black");
    playlist1.insert_at(0, prod1);
    
    //TODO: proper number of tracks, etc...
    tractor.set_track(playlist1, 0);
    tractor.set_track(playlist2, 1);
    tractor.set_track(playlist3, 2);
    xmlConsumer.set("terminate_on_pause", 1);
    Mlt::Producer prod(tractor.get_producer());
    if (!prod.is_valid()) return QString();
    xmlConsumer.connect(prod);
    xmlConsumer.run();
    playlist = QString::fromUtf8(xmlConsumer.get("kdenlive_playlist"));
    return playlist;
}

QString Timeline::toXml() const
{

    Mlt::Profile profile((mlt_profile) 0); //*m_producer->profile()
    Mlt::Consumer xmlConsumer(profile, "xml:kdenlive_playlist");

    Q_ASSERT(xmlConsumer.is_valid());

    //     m_producer->optimise();

    xmlConsumer.set("terminate_on_pause", 1);
    // makes the consumer also store properties we added (with the prefix "kdenlive")
    xmlConsumer.set("store", "kdenlive");
    Mlt::Producer producer(m_producer->get_producer());

    Q_ASSERT(producer.is_valid());

    xmlConsumer.connect(producer);
    xmlConsumer.run();
    return QString::fromUtf8(xmlConsumer.get("kdenlive_playlist"));
}

int Timeline::duration() const
{
    if (m_tractor) return m_tractor->get_playtime();
    return m_producer->get_playtime();
}

Project* Timeline::project()
{
    return m_parent;
}

QList< TimelineTrack* > Timeline::tracks()
{
    return m_tracks;
}

Mlt::Profile* Timeline::profile() const
{
    return m_producer->profile();
}

ProducerWrapper* Timeline::producer()
{
    if (m_clipBinProducer) return m_clipBinProducer;
    return m_producer;
}

int Timeline::position() const
{
    if (!m_producer)
	return 0;
    return m_producer->position();
}

MonitorView* Timeline::monitor()
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
    kDebug() << "PROJECT TRACKS: "<<m_tracks.count()<< " / "<<multitrack->count();
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

void Timeline::loadClip()
{
    Mlt::Multitrack *multitrack = m_tractor->multitrack();
    kDebug() << "PROJECT TRACKS: "<<m_tracks.count()<< " / "<<multitrack->count();
    TimelineTrack *track = new TimelineTrack(new ProducerWrapper(multitrack->track(0)), this);
    if (track) {
        m_tracks.append(track);
    }
}

void Timeline::emitDurationChanged()
{
    emit durationChanged(duration());
}

void Timeline::producer_change(mlt_producer /*producer*/, Timeline* self)
{
    self->emitDurationChanged();
}


#include "timeline.moc"
