/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "timelineclipitem.h"
#include "project/abstracttimelineclip.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>


TimelineClipItem::TimelineClipItem(AbstractTimelineClip* clip, QGraphicsRectItem* parent) :
    QGraphicsRectItem(parent),
    m_clip(clip)
{
    setRect(m_clip->position(), 0, m_clip->duration(), parent->rect().height());

    setBrush(Qt::green);
}

TimelineClipItem::~TimelineClipItem()
{
}


AbstractTimelineClip* TimelineClipItem::clip()
{
    return m_clip;
}

void TimelineClipItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget)

    const QRectF exposed = option->exposedRect;
    painter->setClipRect(exposed);
    painter->fillRect(exposed, brush());
    painter->setWorldMatrixEnabled(false);;
    const QRectF mapped = painter->worldTransform().mapRect(rect());

    // only paint details if clip is big enough
    if (mapped.width() > 20) {
        const QRectF textBounding = painter->boundingRect(mapped, Qt::AlignHCenter | Qt::AlignVCenter, ' ' + m_clip->name() + ' ');
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setBrush(Qt::blue);
        painter->setPen(Qt::NoPen);
        painter->drawRoundedRect(textBounding, 3, 3);
        painter->setBrush(Qt::NoBrush);

        painter->setPen(Qt::white);
        painter->drawText(textBounding, Qt::AlignCenter, m_clip->name());
        painter->setRenderHint(QPainter::Antialiasing, false);
    }

    // draw clip border
    // expand clip rect to allow correct painting of clip border
    painter->setClipping(false);
    painter->drawRect(mapped.adjusted(0, 0, -0.5, 0));
}

#include "timelineclipitem.moc"
