/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef ABSTRACTCLIPITEMTOOL_H
#define ABSTRACTCLIPITEMTOOL_H

#include <QObject>
#include <QPointF>
#include <kdemacros.h>

class TimelineClipItem;
class TimelineScene;
class QEvent;
class QGraphicsSceneMouseEvent;


class KDE_EXPORT AbstractClipItemTool : public QObject
{
    Q_OBJECT

public:
    enum EditingTypes { TrimStart, TrimEnd, SetPosition, NoEditing };

    explicit AbstractClipItemTool(QObject* parent = 0);

    virtual void clipEvent(TimelineClipItem *clipItem, QEvent *event);
//     virtual void mouseOverClip(TimelineScene *scene, TimelineClipItem *clip, QGraphicsSceneMouseEvent *event);

    virtual void hover(TimelineScene *scene, TimelineClipItem *clip, QGraphicsSceneMouseEvent *event);

protected:
    virtual void mouseMove(QGraphicsSceneMouseEvent *event);
    virtual void mousePress(QGraphicsSceneMouseEvent *event);
    virtual void mouseRelease(QGraphicsSceneMouseEvent *event);

    virtual void hoverIn(QGraphicsSceneMouseEvent *event);
    virtual void hoverOut(QGraphicsSceneMouseEvent *event);
    virtual void hoverPosition(QGraphicsSceneMouseEvent *event);

    EditingTypes m_editMode;
    TimelineScene *m_scene;
    TimelineClipItem *m_clip;

private:
    EditingTypes editMode(QPointF position);
};

#endif
