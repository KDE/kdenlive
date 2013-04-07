/*
Copyright (C) 2013  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "trimclipenditemaction.h"
#include "core/timelineview/timelineclipitem.h"
#include "core/project/abstracttimelineclip.h"
#include "core/project/abstractprojectclip.h"
#include <QGraphicsSceneMouseEvent>

    
TrimClipEndItemAction::TrimClipEndItemAction(TimelineClipItem* clipItem, QEvent* initialEvent, QObject* parent) :
    AbstractClipItemAction(clipItem, initialEvent, parent)
{
    slotEvent(initialEvent);
}

TrimClipEndItemAction::~TrimClipEndItemAction()
{
}

void TrimClipEndItemAction::mousePress(QGraphicsSceneMouseEvent* event)
{
    Q_UNUSED(event)

    m_original = m_clip->rect().right();
    m_boundLeft = m_clip->rect().x() + 1;
    AbstractTimelineClip *neighbor = m_clip->clip()->after();
    m_boundRight = neighbor ? neighbor->position() : 0;
    if (m_clip->clip()->projectClip()->hasLimitedDuration()) {
        m_boundRight = qMin(m_original + m_clip->clip()->out(),
                            m_boundRight);
    }
}

void TrimClipEndItemAction::mouseMove(QGraphicsSceneMouseEvent* event)
{
    int position = qMax(m_boundLeft, qRound(event->scenePos().x()));
    if (m_boundRight > m_boundLeft) {
        position = qMin(m_boundRight, position);
    }
    m_clip->setGeometry(m_clip->clip()->position(), position - m_clip->clip()->position());
}

void TrimClipEndItemAction::mouseRelease(QGraphicsSceneMouseEvent* event)
{
    int position = qMax(m_boundLeft, qRound(event->scenePos().x()));
    if (m_boundRight > m_boundLeft) {
        position = qMin(m_boundRight, position);
    }
    m_clip->clip()->setOut(m_clip->clip()->out() + position - m_original);

    emit finished();
}
