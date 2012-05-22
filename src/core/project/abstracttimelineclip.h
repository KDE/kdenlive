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


class KDE_EXPORT AbstractTimelineClip : public QObject
{
    Q_OBJECT

public:
    AbstractTimelineClip(ProducerWrapper *producer, AbstractProjectClip *projectClip, TimelineTrack *parent);
    virtual ~AbstractTimelineClip();

    ProducerWrapper *producer();
    AbstractProjectClip *projectClip();
    TimelineTrack *track();

protected:
    ProducerWrapper *m_producer;
    AbstractProjectClip *m_projectClip;
    TimelineTrack *m_parent;
};

#endif
