/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "scenetool.h"
#include "abstractclipitemtool.h"
#include "timelineview/timelineclipitem.h"
#include "timelineview/timelinescene.h"
#include "timelineview/timelineview.h"
#include <QGraphicsSceneMouseEvent>

#include <KDebug>


SceneTool::SceneTool(QObject* parent) :
    QObject(parent),
    m_clipTool(NULL)
{
}

void SceneTool::setClipTool(AbstractClipItemTool *clipTool)
{
    m_clipTool = clipTool;
}

void SceneTool::clipEvent(TimelineClipItem* clipItem, QEvent* event)
{
    m_clipTool->clipEvent(clipItem, event);
}

void SceneTool::sceneEvent(TimelineScene* scene, QEvent* event)
{
    switch (event->type()) {
        case QEvent::GraphicsSceneMouseMove:
            sceneMouseMove(scene, static_cast<QGraphicsSceneMouseEvent*>(event));
            break;
        default:
            ;
    }
}

void SceneTool::sceneMouseMove(TimelineScene* scene, QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::NoModifier) {
        TimelineClipItem *clip = qgraphicsitem_cast<TimelineClipItem*>(scene->itemAt(event->scenePos(), scene->view()->transform()));
        if (clip) {
            m_clipTool->hover(scene, clip, event);
        } else {
            scene->view()->setCursor(Qt::ArrowCursor);
        }
    }
}

#include "scenetool.moc"

