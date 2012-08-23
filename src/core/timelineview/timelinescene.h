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

/**
 * @class TimelineScene
 * @brief Scene representing the timeline.
 * 
 * Unlike the old system clips are no direct children, but are managed by track items instead.
 */


class KDE_EXPORT TimelineScene : public QGraphicsScene
{
    Q_OBJECT

public:
    enum ItemTypes { TrackItemType = QGraphicsItem::UserType + 1, ClipItemType };

    /** @brief Triggers the creation of the track items and adds the playback position cursor. */
    TimelineScene(Timeline *timeline, ToolManager *toolManager, TimelineView *view, QObject* parent = 0);
    ~TimelineScene();

    /** @brief Returns a pointer to the timeline this scene represents. */
    Timeline *timeline();
    /** @brief Returns a pointer to the view that displays this scene. */
    TimelineView *view();
    /** @brief Returns a pointer to the track item at @param index. */
    TimelineTrackItem *trackItem(int index);
    /** @brief Returns a pointer to the tool manager used by this scene. */
    ToolManager *toolManager();

public slots:
    /**
     * @brief Positions the track items vertically.
     * @param after pointer to the track item whose size changed
     * 
     * emits heightChanged
     */
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
