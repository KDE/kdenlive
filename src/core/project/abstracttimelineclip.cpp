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
#include "timeline.h"
#include "project.h"
#include "commands/resizeclipcommand.h"
#include "commands/moveclipcommand.h"
#include <QUndoStack>


AbstractTimelineClip::AbstractTimelineClip(AbstractProjectClip* projectClip, TimelineTrack* parent, ProducerWrapper* producer) :
    ShiftingProducer(producer, parent),
    m_projectClip(projectClip),
    m_parent(parent)
{
    if (!producer) {
        ProducerWrapper *baseProducer = receiveBaseProducer(parent->index());
        setProducer(new ProducerWrapper(baseProducer->cut()));
    }
}

AbstractTimelineClip::~AbstractTimelineClip()
{
}

AbstractProjectClip* AbstractTimelineClip::projectClip()
{
    return m_projectClip;
}

TimelineTrack* AbstractTimelineClip::track()
{
    return m_parent;
}

int AbstractTimelineClip::index() const
{
    return m_parent->clipIndex(const_cast<AbstractTimelineClip*>(this));
}

AbstractTimelineClip* AbstractTimelineClip::before()
{
    return m_parent->before(this);
}

AbstractTimelineClip* AbstractTimelineClip::after()
{
    return m_parent->after(this);
}

int AbstractTimelineClip::position() const
{
    return m_parent->clipPosition(this);
}

int AbstractTimelineClip::duration() const
{
    return m_producer->get_playtime();
}

int AbstractTimelineClip::in() const
{
    return m_producer->get_in();
}

int AbstractTimelineClip::out() const
{
    return m_producer->get_out();
}

QString AbstractTimelineClip::name() const
{
    return m_projectClip->name();
}

void AbstractTimelineClip::emitResized()
{
    emit resized();
}

void AbstractTimelineClip::emitMoved()
{
    emit moved();
}

void AbstractTimelineClip::setPosition(int position, int track, QUndoCommand* parentCommand)
{
    MoveClipCommand *command = new MoveClipCommand(track >= 0 ? track : m_parent->index(), m_parent->index(), position, this->position(), parentCommand);
    if (!parentCommand) {
        m_parent->timeline()->project()->undoStack()->push(command);
    }
}

void AbstractTimelineClip::setIn(int in, QUndoCommand* parentCommand)
{
    setInOut(in, out(), parentCommand);
}

void AbstractTimelineClip::setOut(int out, QUndoCommand* parentCommand)
{
    setInOut(in(), out, parentCommand);
}

void AbstractTimelineClip::setInOut(int in, int out, QUndoCommand* parentCommand)
{
    ResizeClipCommand *command = new ResizeClipCommand(m_parent->index(), position(), in, out, this->in(), this->out(), parentCommand);
    if (!parentCommand) {
        m_parent->timeline()->project()->undoStack()->push(command);
    }
}

ProducerWrapper* AbstractTimelineClip::receiveBaseProducer(int track) const
{
    Q_UNUSED(track)

    return m_projectClip->timelineBaseProducer();
}

void AbstractTimelineClip::setParent(TimelineTrack* track)
{
    QObject::setParent(track);
    m_parent = track;
}

#include "abstracttimelineclip.moc"
