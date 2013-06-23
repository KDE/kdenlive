/*
Copyright (C) 2013  Jean-Baptiste Mardelle <jb.kdenlive.org>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef WEBVFXTIMELINECLIPITEM_H
#define WEBVFXTIMELINECLIPITEM_H

#include "core/timelineview/timelineclipitem.h"

class WebvfxProjectClip;
class WebvfxTimelineClip;


class WebvfxTimelineClipItem : public TimelineClipItem
{
    Q_OBJECT

public:
    WebvfxTimelineClipItem(WebvfxTimelineClip* clip, QGraphicsItem* parent);

protected:
    void paintBackgroundLayer(QPainter *painter, const QRectF &exposed);

private:
    WebvfxProjectClip *m_projectClip;
};

#endif
