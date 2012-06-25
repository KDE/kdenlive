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


class TimelineView : public QGraphicsView
{
    Q_OBJECT

public:
    TimelineView(QWidget* parent = 0);
    virtual ~TimelineView();

    void setScene(TimelineScene *scene);

    double scale() const;

public slots:
    void setZoom(int level);
    void zoomIn();
    void zoomOut();
    void zoomFit();
    void setHeight(int height);

signals:
    void zoomChanged(int level);

protected:
    void wheelEvent(QWheelEvent *event);
    void mousePressEvent(QMouseEvent *event);

private:
    TimelineScene *m_scene;
    int m_zoomLevel;
};

#endif
