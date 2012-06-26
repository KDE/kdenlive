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

void AbstractTimelineClip::setProducer(ProducerWrapper* producer)
{
    if (m_producer->get_parent() == producer->get_parent()) {
        delete m_producer;
        m_producer = producer;
        emit producerChanged();
    } else {
    }
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

void AbstractTimelineClip::setPosition(int position, QUndoCommand* parentCommand)
{
    MoveClipCommand *command = new MoveClipCommand(m_parent->index(), position, this->position(), parentCommand);
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


#include "abstracttimelineclip.moc"
