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
#include "commands/configuretrackcommand.h"
#include "effectsystem/effectdevice.h"
#include "monitor/monitormodel.h"

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

    m_producerChangeEvent = m_producer->listen("producer-changed", this, (mlt_listener)producer_change);
}

TimelineTrack::~TimelineTrack()
{
    qDeleteAll(m_clips);
    delete m_producerChangeEvent;
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

int TimelineTrack::index() const
{
    return m_parent->tracks().indexOf(const_cast<TimelineTrack*>(this));
}

int TimelineTrack::mltIndex() const
{
    return 0;
}

QList< AbstractTimelineClip* > TimelineTrack::clips()
{
    return m_clips.values();
}

AbstractTimelineClip* TimelineTrack::clip(int index)
{
    return m_clips.value(index);
}

int TimelineTrack::indexOfClip(AbstractTimelineClip* clip) const
{
    return m_clips.key(clip);
}

int TimelineTrack::clipPosition(const AbstractTimelineClip* clip) const
{
    return m_playlist->clip(mlt_whence_relative_start, m_clips.key(const_cast<AbstractTimelineClip*>(clip)));
}

AbstractTimelineClip* TimelineTrack::before(AbstractTimelineClip* clip)
{
    QMap<int, AbstractTimelineClip*>::iterator it = m_clips.find(m_clips.key(clip));
    if (it == m_clips.begin()) {
        return 0;
    } else {
        return (--it).value();
    }
}

AbstractTimelineClip* TimelineTrack::after(AbstractTimelineClip* clip)
{
    QMap<int, AbstractTimelineClip*>::iterator it = m_clips.find(m_clips.key(clip)) + 1;
    if (it == m_clips.end()) {
        return 0;
    } else {
        return it.value();
    }
}

void TimelineTrack::adjustIndices(AbstractTimelineClip* after, int by)
{
    QMap<int, AbstractTimelineClip*> adjusted;
    QMap<int, AbstractTimelineClip*>::iterator it;
    int index;
    if (after) {
        index = m_clips.key(after);
        it = m_clips.find(index) + 1;
        ++index;
    } else {
        index = 0;
        it = m_clips.begin();
    }

    if (by == 0) {
        while (it != m_clips.end()) {
            while (m_playlist->is_blank(index) && index < m_playlist->count() - 1) {
                ++index;
            }
            adjusted.insert(index, it.value());
            ++index;
            it = m_clips.erase(it);
        }
    } else {
        while (it != m_clips.end()) {
            adjusted.insert(it.key() + by, it.value());
            it = m_clips.erase(it);
        }
    }

    m_clips.unite(adjusted);
}

QString TimelineTrack::name() const
{
    return m_producer->property("name");
}

void TimelineTrack::setName(const QString& name)
{
    if (name != this->name()) {
        m_parent->project()->undoStack()->push(new ConfigureTrackCommand("Change track name",
                                                                         index(),
                                                                         "name",
                                                                         name,
                                                                         this->name(),
                                                                         &TimelineTrack::emitNameChanged
                                                                        ));
    }
}

void TimelineTrack::emitNameChanged()
{
    emit nameChanged(name());
}

bool TimelineTrack::isHidden() const
{
    int hideState = m_producer->get_int("hide");
    return hideState == 1 || hideState == 3;
}

void TimelineTrack::hide(bool hide)
{
    int muteValue = isMute() ? 2 : 0;
    int hideValue = hide ? 1 : 0;
    QString value = QString::number(muteValue + hideValue);
    QString oldValue = m_producer->property("hide");
    if (value != oldValue) {
        m_parent->project()->undoStack()->push(new ConfigureTrackCommand(hide ? "Hide Track" : "Show Track",
                                                                         index(),
                                                                         "hide",
                                                                         value,
                                                                         oldValue,
                                                                         &TimelineTrack::emitVisibilityChanged
                                                                        ));
    }
}

void TimelineTrack::emitVisibilityChanged()
{
    emit visibilityChanged(isHidden());
    m_parent->monitor()->refresh();
}


bool TimelineTrack::isMute() const
{
    int hideState = m_producer->get_int("hide");
    return hideState == 2 || hideState == 3;
}

void TimelineTrack::mute(bool mute)
{
    int hideValue = isHidden() ? 1 : 0;
    int muteValue = mute ? 2 : 0;
    QString value = QString::number(muteValue + hideValue);
    QString oldValue = m_producer->property("hide");
    if (value != oldValue) {
        m_parent->project()->undoStack()->push(new ConfigureTrackCommand(mute ? "Mute Track" : "Unmute Track",
                                                                         index(),
                                                                         "hide",
                                                                         value,
                                                                         oldValue,
                                                                         &TimelineTrack::emitAudibilityChanged
                                                                        ));
    }
}

void TimelineTrack::emitAudibilityChanged()
{
    emit audibilityChanged(isMute());
}

bool TimelineTrack::isLocked() const
{
    return m_producer->get_int("locked");
}

void TimelineTrack::lock(bool lock)
{
    if (lock != isLocked()) {
        m_parent->project()->undoStack()->push(new ConfigureTrackCommand(lock ? "Lock Track" : "Unlock Track",
                                                                         index(),
                                                                         "locked",
                                                                         QString::number(lock),
                                                                         m_producer->property("locked"),
                                                                         &TimelineTrack::emitLockStateChanged
                                                                        ));
    }
}

void TimelineTrack::emitLockStateChanged()
{
    emit lockStateChanged(isLocked());
}

void TimelineTrack::emitDurationChanged()
{
    emit durationChanged(producer()->get_playtime());
}

void TimelineTrack::producer_change(mlt_producer producer, TimelineTrack* self)
{
    self->emitDurationChanged();
}

TimelineTrack::Types TimelineTrack::type() const
{
    return static_cast<TimelineTrack::Types>(m_producer->get_int("type"));
}

void TimelineTrack::setType(TimelineTrack::Types type)
{

}

#include "timelinetrack.moc"
