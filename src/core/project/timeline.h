/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef TIMELINE_H
#define TIMELINE_H

#include <QObject>
#include <kdemacros.h>

class Project;
class ProducerWrapper;
class TimelineTrack;
namespace Mlt
{
    class Tractor;
    class Profile;
}


class KDE_EXPORT Timeline : public QObject
{
    Q_OBJECT

public:
    Timeline(const QString &document, Project* parent);
    virtual ~Timeline();

    Project *project();
    QList<TimelineTrack *> tracks();
    Mlt::Profile *profile();
    ProducerWrapper *producer();

    void loadTracks();

private:
    Project *m_parent;
    Mlt::Tractor *m_tractor;
    ProducerWrapper *m_producer;
    Mlt::Profile *m_profile;
    QList <TimelineTrack *> m_tracks;
    
};

#endif
