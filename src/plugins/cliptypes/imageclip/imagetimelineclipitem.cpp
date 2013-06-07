/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "imagetimelineclipitem.h"
#include "imagetimelineclip.h"
#include "imageprojectclip.h"
#include "core/project/abstractprojectclip.h"
#include <QPainter>


ImageTimelineClipItem::ImageTimelineClipItem(ImageTimelineClip* clip, QGraphicsItem* parent) :
    TimelineClipItem(clip,parent)
{
    m_projectClip = static_cast<ImageProjectClip*>(clip->projectClip());
    setBrush(QColor(141, 166, 215));
}

void ImageTimelineClipItem::paintBackgroundLayer(QPainter* painter, const QRectF &exposed)
{
    painter->setWorldMatrixEnabled(false);
    const QRectF mapped = painter->worldTransform().mapRect(rect());
    
    QPixmap thumbnail = m_projectClip->thumbnail();
    if (!thumbnail.isNull()) {
        thumbnail = thumbnail.scaledToHeight(mapped.height());

        for (int i = mapped.left(); i < mapped.right(); i += thumbnail.width()) {
            painter->drawPixmap(i, mapped.top(), thumbnail);
        }
    } else {
        painter->drawRect(exposed);
    }
}

#include "imagetimelineclipitem.moc"
