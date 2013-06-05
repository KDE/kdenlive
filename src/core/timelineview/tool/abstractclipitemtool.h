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

class AbstractClipItemAction;
class TimelineClipItem;
class TimelineScene;
class QGraphicsSceneMouseEvent;


class KDE_EXPORT AbstractClipItemTool : public QObject
{
    Q_OBJECT

public:
    enum EditingTypes { TrimStart, TrimEnd, SetPosition, NoEditing };

    explicit AbstractClipItemTool(QObject* parent = 0);

    virtual void clipEvent(TimelineClipItem *clipItem, QEvent *event) = 0;

    virtual void hover(TimelineScene *scene, TimelineClipItem *clip, QGraphicsSceneMouseEvent *event);

public slots:
    void slotActionFinished();

protected:
    virtual void hoverIn(QGraphicsSceneMouseEvent *event);
    virtual void hoverOut(QGraphicsSceneMouseEvent *event);
    virtual void hoverPosition(QGraphicsSceneMouseEvent *event);

    EditingTypes m_editMode;
    TimelineScene *m_scene;
    TimelineClipItem *m_clip;
    AbstractClipItemAction *m_editAction;

private:
    EditingTypes editMode(const QPointF &position);
};

#endif
