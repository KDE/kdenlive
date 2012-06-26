/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef TIMELINEBACKGROUND_H
#define TIMELINEBACKGROUND_H

#include <QObject>

class Timeline;
class ProducerWrapper;

namespace Mlt
{
    class Playlist;
}


class TimelineBackground : public QObject
{
    Q_OBJECT

public:
    explicit TimelineBackground(ProducerWrapper *track, Timeline *parent);
    virtual ~TimelineBackground();

private slots:
    void onTimelineDurationChange();

private:
    Timeline *m_parent;
    Mlt::Playlist *m_playlist;
};

#endif
