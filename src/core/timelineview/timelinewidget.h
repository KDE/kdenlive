/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#ifndef TIMELINEWIDGET_H
#define TIMELINEWIDGET_H

#include <QWidget>

class Project;
class TimelineScene;
class TimelinePositionBar;
class QGraphicsView;


class TimelineWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TimelineWidget(QWidget* parent = 0);
    virtual ~TimelineWidget();

public slots:
    void setProject(Project *project);

    TimelineScene *scene();
    QGraphicsView *view();

private:
    TimelineScene *m_scene;
    QGraphicsView *m_view;
    TimelinePositionBar *m_positionBar;
};

#endif
