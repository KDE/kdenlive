/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef TIMELINECLIPITEM_H
#define TIMELINECLIPITEM_H

#include <QGraphicsRectItem>

class AbstractTimelineClip;


class TimelineClipItem : public QObject, public QGraphicsRectItem
{
    Q_OBJECT

public:
    TimelineClipItem(AbstractTimelineClip *clip, QGraphicsRectItem* parent = 0);
    virtual ~TimelineClipItem();

    AbstractTimelineClip *clip();

    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

private:
    AbstractTimelineClip *m_clip;
};

#endif
