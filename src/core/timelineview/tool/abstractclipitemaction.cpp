/*
Copyright (C) 2013  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "abstractclipitemaction.h"
#include "timelineview/timelineclipitem.h"
#include <QGraphicsSceneMouseEvent>


AbstractClipItemAction::AbstractClipItemAction(TimelineClipItem* clipItem, QEvent* initialEvent, QObject* parent) :
    QObject(parent),
    m_clip(clipItem)
{
    connect(m_clip, SIGNAL(signalEvent(QEvent*)), this, SLOT(slotEvent(QEvent*)));
}

AbstractClipItemAction::~AbstractClipItemAction()
{
}

void AbstractClipItemAction::slotEvent(QEvent* event)
{
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

void AbstractClipItemAction::mouseMove(QGraphicsSceneMouseEvent* event)
{
}

void AbstractClipItemAction::mousePress(QGraphicsSceneMouseEvent* event)
{
}

void AbstractClipItemAction::mouseRelease(QGraphicsSceneMouseEvent* event)
{
}
