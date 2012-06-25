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
#include <QGraphicsItem>
#include <kdemacros.h>

class Timeline;
class TimelineTrackItem;
class ToolManager;
class TimelineView;


class KDE_EXPORT TimelineScene : public QGraphicsScene
{
    Q_OBJECT

public:
    enum ItemTypes { TrackItemType = QGraphicsItem::UserType + 1, ClipItemType };

    TimelineScene(Timeline *timeline, ToolManager *toolManager, TimelineView *view, QObject* parent = 0);
    ~TimelineScene();

    Timeline *timeline();
    TimelineView *view();
    TimelineTrackItem *trackItem(int index);
    ToolManager *toolManager();

public slots:
    void positionTracks(TimelineTrackItem *after = 0);

signals:
    void heightChanged(int height);

protected:
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

private:
    void setupTimeline();

    Timeline *m_timeline;
    TimelineView *m_view;
    QList <TimelineTrackItem*> m_trackItems;
    ToolManager *m_toolManager;
};

#endif
