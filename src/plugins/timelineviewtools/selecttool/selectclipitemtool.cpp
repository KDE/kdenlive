/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "selectclipitemtool.h"
#include "setclippostionitemaction.h"
#include "trimclipenditemaction.h"
#include "trimclipstartitemaction.h"
#include "core/timelineview/timelineclipitem.h"
#include <QGraphicsSceneMouseEvent>


SelectClipItemTool::SelectClipItemTool(QObject* parent) :
    AbstractClipItemTool(parent)
{
}

void SelectClipItemTool::clipEvent(TimelineClipItem* clipItem, QEvent* event)
{
    if (event->type() == QEvent::GraphicsSceneMousePress && !m_editAction) {
        QGraphicsSceneMouseEvent *sceneEvent = static_cast<QGraphicsSceneMouseEvent*>(event);
        
        if (sceneEvent->button() == Qt::LeftButton) {
            switch (m_editMode) {
                case TrimStart:
                    m_editAction = new TrimClipStartItemAction(clipItem, sceneEvent, this);
                break;
                case TrimEnd:
                    m_editAction = new TrimClipEndItemAction(clipItem, sceneEvent, this);
                    break;
                case SetPosition:
                    m_editAction = new SetClipPositionItemAction(clipItem, sceneEvent, this);
                    break;
                default:
                    return;
            }

            if (m_editAction) {
                connect(m_editAction, SIGNAL(finished()), this, SLOT(slotActionFinished()));
            }
        } else {
            m_editMode = NoEditing;
        }
    }
}

#include "selectclipitemtool.moc"
