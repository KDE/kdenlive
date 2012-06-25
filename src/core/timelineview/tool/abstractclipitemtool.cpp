/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "abstractclipitemtool.h"
#include "timelineview/timelineview.h"
#include "timelineview/timelineclipitem.h"
#include "project/abstracttimelineclip.h"
#include <KCursor>
#include <QGraphicsSceneMouseEvent>


AbstractClipItemTool::AbstractClipItemTool(QObject* parent) :
    QObject(parent),
    m_editMode(NoEditing),
    m_scene(NULL),
    m_clip(NULL)
{

}

void AbstractClipItemTool::clipEvent(TimelineClipItem* clipItem, QEvent* event)
{
    m_clip = clipItem;

    switch (event->type()) {
        case QEvent::GraphicsSceneMouseMove:
            mouseMove(static_cast<QGraphicsSceneMouseEvent*>(event));
            break;
        case QEvent::GraphicsSceneMousePress:
            mousePress(static_cast<QGraphicsSceneMouseEvent*>(event));
            break;
        case QEvent::GraphicsSceneMouseRelease:
            mouseRelease(static_cast<QGraphicsSceneMouseEvent*>(event));
            break;
        default:
            ;
    }
    event->accept();
}

void AbstractClipItemTool::hover(TimelineScene* scene, TimelineClipItem* clip, QGraphicsSceneMouseEvent* event)
{
    m_scene = scene;
    m_clip = clip;

    double scale = scene->view()->scale();
    double maximumOffset = 6 / scale;

    QRectF rect = clip->sceneBoundingRect();

    m_editMode = editMode(event->scenePos());

    switch (m_editMode) {
        case TrimStart:
            hoverIn(event);
            break;
        case TrimEnd:
            hoverOut(event);
            break;
        case SetPosition:
            hoverPosition(event);
            break;
    }
}

void AbstractClipItemTool::mouseMove(QGraphicsSceneMouseEvent* event)
{
}

void AbstractClipItemTool::mousePress(QGraphicsSceneMouseEvent* event)
{
}

void AbstractClipItemTool::mouseRelease(QGraphicsSceneMouseEvent* event)
{
}


void AbstractClipItemTool::hoverIn(QGraphicsSceneMouseEvent* event)
{
    m_scene->view()->setCursor(KCursor("left_side", Qt::SizeHorCursor));
}

void AbstractClipItemTool::hoverOut(QGraphicsSceneMouseEvent* event)
{
    m_scene->view()->setCursor(KCursor("right_side", Qt::SizeHorCursor));
}

void AbstractClipItemTool::hoverPosition(QGraphicsSceneMouseEvent* event)
{
    m_scene->view()->setCursor(Qt::OpenHandCursor);
}

AbstractClipItemTool::EditingTypes AbstractClipItemTool::editMode(QPointF position)
{
    double scale = m_scene->view()->scale();
    double maximumOffset = 6 / scale;

    QRectF rect = m_clip->sceneBoundingRect();

    if (position.x() - rect.x() < maximumOffset) {
        return TrimStart;
    } else if (rect.right() - position.x() < maximumOffset) {
        return TrimEnd;
    } else {
        return SetPosition;
    }
}
