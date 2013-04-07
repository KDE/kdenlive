/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef TIMELINEVIEW_H
#define TIMELINEVIEW_H

#include <QGraphicsView>

class TimelineScene;

/**
 * @class TimelineView
 * @brief Displays the timeline scene and takes care of related actions, like zooming.
 */


class TimelineView : public QGraphicsView
{
    Q_OBJECT

public:
    TimelineView(QWidget* parent = 0);
    virtual ~TimelineView();

    /** @brief Sets the view to display @param scene. */
    void setScene(TimelineScene *scene);

    /** @brief Returns the horizontal scale factor applied on the view. */
    double scale() const;

public slots:
    /**
     * @brief Sets the horizontal zoom.
     * @param level zoom level @see TimelinePositionBar
     * 
     * emits zoomChanged
     */
    void setZoom(int level);
    /** @brief Zooms in by one step. */
    void zoomIn();
    /** @brief Zooms out by one step. */
    void zoomOut();
    /** @brief Makes the timeline fit into the view horizontally. */
    void zoomFit();

signals:
    void zoomChanged(int level);

protected:
    void wheelEvent(QWheelEvent *event);
    void mousePressEvent(QMouseEvent *event);

private slots:
    void setHeight(int height);

private:
    TimelineScene *m_scene;
    int m_zoomLevel;
};

#endif
