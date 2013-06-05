/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef SCENETOOL_H
#define SCENETOOL_H

#include <QObject>
#include <kdemacros.h>

class AbstractClipItemTool;
class TimelineClipItem;
class TimelineScene;
class QEvent;
class QGraphicsSceneMouseEvent;


class KDE_EXPORT SceneTool : public QObject
{
    Q_OBJECT

public:
    explicit SceneTool(QObject* parent = 0);

    void setClipTool(AbstractClipItemTool *clipTool);

    virtual void clipEvent(TimelineClipItem *clipItem, QEvent *event);
    virtual void sceneEvent(TimelineScene *scene, QEvent *event);

protected:
    virtual void sceneMouseMove(TimelineScene *scene, QGraphicsSceneMouseEvent *event);

private:
    AbstractClipItemTool *m_clipTool;
};

#endif
