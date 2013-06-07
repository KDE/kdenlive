/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef GENERICTIMELINECLIPITEM_H
#define GENERICTIMELINECLIPITEM_H

#include "core/timelineview/timelineclipitem.h"

class GenericProjectClip;
class GenericTimelineClip;


class GenericTimelineClipItem : public TimelineClipItem
{
    Q_OBJECT

public:
    GenericTimelineClipItem(GenericTimelineClip* clip, QGraphicsItem* parent);

protected:
    void paintBackgroundLayer(QPainter *painter, const QRectF &exposed);

private:
    GenericProjectClip *m_projectClip;
};

#endif
