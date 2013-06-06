/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "abstractclipitemtool.h"
#include "abstractclipitemaction.h"
#include "timelineview/timelineview.h"
#include "timelineview/timelineclipitem.h"
#include "project/abstracttimelineclip.h"
#include <KCursor>
#include <QGraphicsSceneMouseEvent>


AbstractClipItemTool::AbstractClipItemTool(QObject* parent) :
    QObject(parent),
    m_editMode(NoEditing),
    m_scene(NULL),
    m_clip(NULL),
    m_editAction(NULL)
{

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
    default:
        // Should we do something?
        break;
    }
}

void AbstractClipItemTool::hoverIn(QGraphicsSceneMouseEvent* event)
{
    Q_UNUSED(event)

    m_scene->view()->setCursor(KCursor("left_side", Qt::SizeHorCursor));
}

void AbstractClipItemTool::hoverOut(QGraphicsSceneMouseEvent* event)
{
    Q_UNUSED(event)

    m_scene->view()->setCursor(KCursor("right_side", Qt::SizeHorCursor));
}

void AbstractClipItemTool::hoverPosition(QGraphicsSceneMouseEvent* event)
{
    Q_UNUSED(event)

    m_scene->view()->setCursor(Qt::OpenHandCursor);
}

AbstractClipItemTool::EditingTypes AbstractClipItemTool::editMode(const QPointF &position)
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

void AbstractClipItemTool::slotActionFinished()
{
    delete m_editAction;
    m_editAction = 0;
    m_editMode = NoEditing;
}

#include "abstractclipitemtool.moc"
