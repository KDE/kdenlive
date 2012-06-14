/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "positionitem.h"
#include "timelinescene.h"
#include "project/timeline.h"
#include "monitor/monitormodel.h"


PositionItem::PositionItem(TimelineScene* scene) :
    QGraphicsLineItem()
{
    int position = scene->timeline()->monitor()->position();
    setLine(position, 0, position, scene->height());

    connect(scene, SIGNAL(heightChanged(int)), this, SLOT(setHeight(int)));
    connect(scene->timeline()->monitor(), SIGNAL(positionChanged(int)), this, SLOT(setPosition(int)));
}

void PositionItem::setPosition(int position)
{
    setX(position);
}

void PositionItem::setHeight(int height)
{
    setLine(x(), 0, x(), height);
}
