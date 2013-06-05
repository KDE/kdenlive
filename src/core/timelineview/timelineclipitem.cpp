/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "timelineclipitem.h"
#include "timelinetrackitem.h"
#include "timelinescene.h"
#include "tool/toolmanager.h"
#include "project/abstracttimelineclip.h"
#include "project/timelinetrack.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>
#include <QGraphicsSceneMouseEvent>


TimelineClipItem::TimelineClipItem(AbstractTimelineClip* clip, QGraphicsItem* parent) :
    QGraphicsRectItem(parent),
    m_clip(clip)
{
    updateGeometry(false);

    connect(m_clip, SIGNAL(resized()), this, SLOT(updateGeometry()));
    connect(m_clip, SIGNAL(moved()), this, SLOT(updateGeometry()));

    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
}

TimelineClipItem::~TimelineClipItem()
{
}

int TimelineClipItem::type() const
{
    return Type;
}

AbstractTimelineClip* TimelineClipItem::clip()
{
    return m_clip;
}

void TimelineClipItem::setGeometry(int position, int duration)
{
    setRect(position, 0, duration, static_cast<QGraphicsRectItem*>(parentItem())->rect().height());
}

void TimelineClipItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget)

    const QRectF exposed = option->exposedRect;
    painter->setClipRect(exposed);
    painter->fillRect(exposed, brush());

    paintBackgroundLayer(painter, exposed);

    painter->setWorldMatrixEnabled(false);
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
    painter->setPen(Qt::blue);
    painter->drawRect(mapped.adjusted(0, 0, -0.5, 0));
}

void TimelineClipItem::paintBackgroundLayer(QPainter* painter, QRectF exposed)
{
}

void TimelineClipItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    static_cast<TimelineScene*>(scene())->toolManager()->clipEvent(this, event);
    emit signalEvent(event);
}

void TimelineClipItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
{
    static_cast<TimelineScene*>(scene())->toolManager()->clipEvent(this, event);
    emit signalEvent(event);
}

void TimelineClipItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
    static_cast<TimelineScene*>(scene())->toolManager()->clipEvent(this, event);
    emit signalEvent(event);
}

void TimelineClipItem::updateGeometry(bool updateTrack)
{
    if (updateTrack) {
        setParentItem(static_cast<TimelineScene*>(scene())->trackItem(m_clip->track()->index()));
    }
    setGeometry(m_clip->position(), m_clip->duration());
}

#include "timelineclipitem.moc"
