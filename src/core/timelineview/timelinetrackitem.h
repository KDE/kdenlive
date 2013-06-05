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

#include "timelinescene.h"
#include <QGraphicsRectItem>
#include <kdemacros.h>

class TimelineTrack;
class TimelineClipItem;

/**
 * @class TimelineTrackItem
 * @brief Represents a track.
 */


class KDE_EXPORT TimelineTrackItem : public QObject, public QGraphicsRectItem
{
    Q_OBJECT

public:
    TimelineTrackItem(TimelineTrack *track, QObject* parent = 0);
    virtual ~TimelineTrackItem();

    enum { Type = TimelineScene::TrackItemType };

    int type() const;

    TimelineTrack *track();

private slots:
    void updateGeometry();

private:
    void loadClips();

    TimelineTrack *m_track;
    QList<TimelineClipItem*> m_clipItems;
};

#endif
