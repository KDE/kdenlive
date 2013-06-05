/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "generictimelineclipitem.h"
#include "generictimelineclip.h"
#include "genericprojectclip.h"
#include "core/project/abstractprojectclip.h"
#include <QPainter>
#include <KDebug>

GenericTimelineClipItem::GenericTimelineClipItem(GenericTimelineClip* clip, QGraphicsItem* parent) :
    TimelineClipItem(clip,parent)
{
    m_projectClip = static_cast<GenericProjectClip*>(clip->projectClip());
    setBrush(QColor(clip->projectClip()->url().fileName()));
}

void GenericTimelineClipItem::paintBackgroundLayer(QPainter* painter, QRectF exposed)
{
    painter->drawRect(exposed);
}
