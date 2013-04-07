/*
Copyright (C) 2013  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "trimclipstartitemaction.h"
#include "core/core.h"
#include "core/timelineview/timelineclipitem.h"
#include "core/project/projectmanager.h"
#include "core/project/project.h"
#include "core/project/abstracttimelineclip.h"
#include "core/project/abstractprojectclip.h"
#include <KLocalizedString>
#include <QGraphicsSceneMouseEvent>
#include <QUndoCommand>


TrimClipStartItemAction::TrimClipStartItemAction(TimelineClipItem* clipItem, QEvent* initialEvent, QObject* parent) :
    AbstractClipItemAction(clipItem, initialEvent, parent)
{
    slotEvent(initialEvent);
}

TrimClipStartItemAction::~TrimClipStartItemAction()
{
}

void TrimClipStartItemAction::mousePress(QGraphicsSceneMouseEvent* event)
{
    Q_UNUSED(event)

    m_original = m_clip->rect().x();
    m_boundRight = m_clip->rect().right() - 1;
    AbstractTimelineClip *neighbor = m_clip->clip()->before();
    m_boundLeft = qMax(m_clip->clip()->projectClip()->hasLimitedDuration() ? m_original - m_clip->clip()->in() : 0,
                                   neighbor ? neighbor->position() + neighbor->duration() : 0);
}

void TrimClipStartItemAction::mouseMove(QGraphicsSceneMouseEvent* event)
{
    int position = qBound(m_boundLeft, qRound(event->scenePos().x()), m_boundRight);
    m_clip->setGeometry(position, m_clip->clip()->duration() + m_original - position);
}

void TrimClipStartItemAction::mouseRelease(QGraphicsSceneMouseEvent* event)
{
    int position = qBound(m_boundLeft, qRound(event->scenePos().x()), m_boundRight);
    QUndoCommand *command = new QUndoCommand();
    command->setText(i18n("Resize Clip"));
    if (position > m_original) {
        m_clip->clip()->setIn(m_clip->clip()->in() + position - m_original, command);
        m_clip->clip()->setPosition(position, -1, command);
    } else {
        m_clip->clip()->setPosition(position, -1, command);
        m_clip->clip()->setIn(m_clip->clip()->in() + position - m_original, command);
    }
    pCore->projectManager()->current()->undoStack()->push(command);
    emit finished();
}
