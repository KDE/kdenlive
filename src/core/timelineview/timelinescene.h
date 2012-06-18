/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef TIMELINESCENE_H
#define TIMELINESCENE_H

#include <QGraphicsScene>
#include <kdemacros.h>

class Timeline;
class TimelineTrackItem;


class KDE_EXPORT TimelineScene : public QGraphicsScene
{
    Q_OBJECT

public:
    TimelineScene(Timeline *timeline, QObject* parent = 0);
    ~TimelineScene();

    Timeline *timeline();
    TimelineTrackItem *trackItem(int index);

public slots:
    void positionTracks(TimelineTrackItem *after = 0);

signals:
    void heightChanged(int height);

private:
    void setupTimeline();

    Timeline *m_timeline;
    QList <TimelineTrackItem*> m_trackItems;
};

#endif
