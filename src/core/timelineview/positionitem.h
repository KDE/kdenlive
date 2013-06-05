/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of Kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef POSITIONITEM_H
#define POSITIONITEM_H

#include <QObject>
#include <QGraphicsLineItem>

class TimelineScene;


/**
 * @class PositionItem
 * @brief Indicates the current playback position with a vertical line in the timeline view.
 */


class PositionItem : public QObject, public QGraphicsLineItem
{
    Q_OBJECT

public:
    /**
     * @brief Creates the line at the current playback position with the required height and sets up signal/slot connections to take of position changes.
     */
    explicit PositionItem(TimelineScene *scene);

private slots:
    void setPosition(int position);
    void setHeight(int height);
};

#endif
