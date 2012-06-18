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
class TimelineView;
class TimelineScene;
class TimelinePositionBar;
class TrackHeaderContainer;


class TimelineWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TimelineWidget(QWidget* parent = 0);
    virtual ~TimelineWidget();

    TimelineView *view();
    TimelineScene *scene();

public slots:
    void setProject(Project *project);

private:
    TimelineScene *m_scene;
    TimelineView *m_view;
    TimelinePositionBar *m_positionBar;
    TrackHeaderContainer *m_headerContainer;
};

#endif
