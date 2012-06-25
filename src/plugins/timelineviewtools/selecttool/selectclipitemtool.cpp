/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "selectclipitemtool.h"
#include "core/core.h"
#include "core/timelineview/timelineclipitem.h"
#include "core/project/projectmanager.h"
#include "core/project/project.h"
#include "core/project/abstracttimelineclip.h"
#include "core/project/abstractprojectclip.h"
#include <KLocalizedString>
#include <QGraphicsSceneMouseEvent>
#include <QUndoCommand>

#include <KDebug>


SelectClipItemTool::SelectClipItemTool(QObject* parent) :
    AbstractClipItemTool(parent)
{
}

void SelectClipItemTool::mouseMove(QGraphicsSceneMouseEvent* event)
{
    int position;
    switch (m_editMode) {
        case TrimStart:
            position = qBound(m_boundLeft, qRound(event->scenePos().x()), m_boundRight);
            m_clip->setGeometry(position, m_clip->clip()->duration() + m_original - position);
            break;
        case TrimEnd:
            position = qMax(m_boundLeft, qRound(event->scenePos().x()));
            if (m_boundRight > m_boundLeft) {
                position = qMin(m_boundRight, position);
            }
            m_clip->setGeometry(m_clip->clip()->position(), position - m_clip->clip()->position());
            break;
        case SetPosition:
            break;
    }
}

void SelectClipItemTool::mousePress(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        AbstractTimelineClip *neighbor;
        switch (m_editMode) {
            case TrimStart:
                m_original = m_clip->rect().x();
                m_boundRight = m_clip->rect().right() - 1;
                neighbor = m_clip->clip()->before();
                m_boundLeft = qMax(m_clip->clip()->projectClip()->hasLimitedDuration() ? m_original - m_clip->clip()->in() : 0,
                                   neighbor ? neighbor->position() + neighbor->duration() : 0);
                break;
            case TrimEnd:
                m_original = m_clip->rect().right();
                m_boundLeft = m_clip->rect().x() + 1;
                neighbor = m_clip->clip()->after();
                m_boundRight = neighbor ? neighbor->position() : 0;
                if (m_clip->clip()->projectClip()->hasLimitedDuration()) {
                    m_boundRight = qMin(m_original + m_clip->clip()->out(),
                                        m_boundRight);
                }
                break;
            case SetPosition:
                break;
            default:
                ;
        }
    } else {
        m_editMode = NoEditing;
    }
}

void SelectClipItemTool::mouseRelease(QGraphicsSceneMouseEvent* event)
{
    QUndoCommand *command;
    int position;
    switch (m_editMode) {
        case TrimStart:
            position = qBound(m_boundLeft, qRound(event->scenePos().x()), m_boundRight);
            command = new QUndoCommand();
            command->setText(i18n("Resize Clip"));
            if (position > m_original) {
                m_clip->clip()->setIn(m_clip->clip()->in() + position - m_original, command);
                m_clip->clip()->setPosition(position, command);
            } else {
                m_clip->clip()->setPosition(position, command);
                m_clip->clip()->setIn(m_clip->clip()->in() + position - m_original, command);
            }
            pCore->projectManager()->current()->undoStack()->push(command);
            break;
        case TrimEnd:
            position = qMax(m_boundLeft, qRound(event->scenePos().x()));
            if (m_boundRight > m_boundLeft) {
                position = qMin(m_boundRight, position);
            }
            m_clip->clip()->setOut(m_clip->clip()->out() + position - m_original);
            break;
        case SetPosition:
            break;
    }
    m_editMode = NoEditing;
}
