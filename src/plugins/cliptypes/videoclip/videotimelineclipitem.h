/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef VIDEOTIMELINECLIPITEM_H
#define VIDEOTIMELINECLIPITEM_H

#include "core/timelineview/timelineclipitem.h"

class VideoProjectClip;
class VideoTimelineClip;


class VideoTimelineClipItem : public TimelineClipItem
{
    Q_OBJECT

public:
    VideoTimelineClipItem(VideoTimelineClip* clip, QGraphicsItem* parent);

protected:
    void paintBackgroundLayer(QPainter *painter, QRectF exposed);

private:
    VideoProjectClip *m_projectClip;
};

#endif
