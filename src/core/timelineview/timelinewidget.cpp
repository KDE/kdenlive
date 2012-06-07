/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "timelinewidget.h"
#include "timelinescene.h"
#include "timelinepositionbar.h"
#include "project/project.h"
#include "project/projectmanager.h"
#include "core.h"
#include <QGraphicsView>
#include <QGridLayout>


TimelineWidget::TimelineWidget(QWidget* parent) :
    QWidget(parent),
    m_scene(NULL)
{
    QGridLayout *layout = new QGridLayout(this);

    m_positionBar = new TimelinePositionBar(this);
    layout->addWidget(m_positionBar, 0, 0);

    m_view = new QGraphicsView(this);
    m_view->setFrameShape(QFrame::NoFrame);
    layout->addWidget(m_view, 1, 0);


    connect(pCore->projectManager(), SIGNAL(projectOpened(Project*)), this, SLOT(setProject(Project*)));
}

TimelineWidget::~TimelineWidget()
{
    delete m_scene;
}

void TimelineWidget::setProject(Project* project)
{
    if (m_scene) {
        delete m_scene;
        m_scene = NULL;
    }
    if (project) {
        m_scene = new TimelineScene(project->timeline(), this);
    }
    m_view->setScene(m_scene);
}

TimelineScene* TimelineWidget::scene()
{
    return m_scene;
}

QGraphicsView* TimelineWidget::view()
{
    return m_view;
}

#include "timelinewidget.moc"
