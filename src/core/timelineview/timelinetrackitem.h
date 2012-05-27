/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef TIMELINETRACKITEM_H
#define TIMELINETRACKITEM_H

#include <QGraphicsRectItem>

class TimelineTrack;
class TimelineClipItem;


class TimelineTrackItem : public QObject, public QGraphicsRectItem
{
    Q_OBJECT

public:
    TimelineTrackItem(TimelineTrack *track, QObject* parent = 0);
    virtual ~TimelineTrackItem();

    TimelineTrack *track();

public slots:
    void adjustLength();

private:
    void loadClips();

    TimelineTrack *m_track;
    QList<TimelineClipItem*> m_clipItems;
};

#endif
