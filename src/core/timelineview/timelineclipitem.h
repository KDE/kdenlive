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
#include <kdemacros.h>
#include "timelinescene.h"

class AbstractTimelineClip;
class QEvent;

/**
 * @class TimelineClipItem
 * @brief Represents a timeline clip in the timeline view.
 * 
 * Can be subclassed in order to implement custom drawing routines (e.g. to draw thumbnails in the background "layer").
 */


class KDE_EXPORT TimelineClipItem : public QObject, public QGraphicsRectItem
{
    Q_OBJECT

public:
    /** @brief Basic setup and connection to the timeline clip's signals. */
    TimelineClipItem(AbstractTimelineClip *clip, QGraphicsItem* parent);
    virtual ~TimelineClipItem();

    enum { Type = TimelineScene::ClipItemType };

    int type() const;

    /** @brief Returns the timeline clip the item represents. */
    AbstractTimelineClip *clip();

    /** @brief Sets the item's geometry.
     * @param position position of the item (in frames/timeline scene pixels)
     * @param duration duration/length of the item (in frames/timelinescene pixels)
     * 
     * WARNING: Using this function to position the item does not change the clip it represents.
     */
    void setGeometry(int position, int duration);

    /** @brief Basic painting. Calls the different "paint layer" functions. */
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget = 0);

public slots:
    /** @brief Synchronizes the item's geometry with the clip's position and duration. */
    void updateGeometry();

protected:
    /** @brief Subclass this function for painting in the background on top of the background color.
     * @param painter painter to be used
     * @param exposed exposed part of the item
     * 
     * No flags are set before calling this function so you might have to disable the world matrix in there.
     * When subclassing use this function to paint thumbnails.
     */
    virtual void paintBackgroundLayer(QPainter *painter, QRectF exposed);

    /** @brief Passes the event on the tool manager. */
    void mouseMoveEvent(QGraphicsSceneMouseEvent *event);
    /** @brief Passes the event on the tool manager. */
    void mousePressEvent(QGraphicsSceneMouseEvent *event);
    /** @brief Passes the event on the tool manager. */
    void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

private:
    AbstractTimelineClip *m_clip;
};

#endif
