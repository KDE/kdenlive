/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef IMAGETIMELINECLIPITEM_H
#define IMAGETIMELINECLIPITEM_H

#include "core/timelineview/timelineclipitem.h"

class ImageProjectClip;
class ImageTimelineClip;


class ImageTimelineClipItem : public TimelineClipItem
{
    Q_OBJECT

public:
    ImageTimelineClipItem(ImageTimelineClip* clip, QGraphicsItem* parent);

protected:
    void paintBackgroundLayer(QPainter *painter, QRectF exposed);

private:
    ImageProjectClip *m_projectClip;
};

#endif
