/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef ABSTRACTTIMELINECLIP_H
#define ABSTRACTTIMELINECLIP_H

#include <QObject>
#include <kdemacros.h>

class ProducerWrapper;
class AbstractProjectClip;
class TimelineTrack;
class QUndoCommand;


class KDE_EXPORT AbstractTimelineClip : public QObject
{
    Q_OBJECT

public:
    AbstractTimelineClip(ProducerWrapper *producer, AbstractProjectClip *projectClip, TimelineTrack *parent);
    virtual ~AbstractTimelineClip();

    ProducerWrapper *producer();
    AbstractProjectClip *projectClip();
    TimelineTrack *track();
    int index() const;

    AbstractTimelineClip *before();
    AbstractTimelineClip *after();

    int position() const;
    int duration() const;
    int in() const;
    int out() const;

    QString name() const;

    void emitResized();
    void emitMoved();

public slots:
    void setPosition(int position, QUndoCommand *parentCommand = 0);
    virtual void setIn(int in, QUndoCommand *parentCommand = 0);
    virtual void setOut(int out, QUndoCommand *parentCommand = 0);
    virtual void setInOut(int in, int out, QUndoCommand *parentCommand = 0);

signals:
    void resized();
    void moved();

protected:
    ProducerWrapper *m_producer;
    AbstractProjectClip *m_projectClip;
    TimelineTrack *m_parent;
};

#endif
