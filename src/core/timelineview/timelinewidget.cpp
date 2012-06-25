/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "timelinewidget.h"
#include "timelineview.h"
#include "timelinescene.h"
#include "timelinepositionbar.h"
#include "trackheadercontainer.h"
#include "tool/toolmanager.h"
#include "project/project.h"
#include "project/projectmanager.h"
#include "core.h"
#include <QGridLayout>
#include <QScrollBar>


TimelineWidget::TimelineWidget(QWidget* parent) :
    QWidget(parent),
    m_scene(NULL)
{
    QGridLayout *layout = new QGridLayout(this);
    layout->setSpacing(0);

    m_positionBar = new TimelinePositionBar(this);
    layout->addWidget(m_positionBar, 0, 1);

    m_headerContainer = new TrackHeaderContainer(this);
    layout->addWidget(m_headerContainer, 1, 0);

    m_view = new TimelineView(this);
    layout->addWidget(m_view, 1, 1);

    m_toolManager = new ToolManager(this);


    connect(m_view, SIGNAL(zoomChanged(int)), m_positionBar, SLOT(setZoomLevel(int)));
    connect(m_view->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_positionBar, SLOT(setOffset(int)));

    connect(m_view->horizontalScrollBar(), SIGNAL(rangeChanged(int,int)), m_headerContainer, SLOT(adjustHeight(int,int)));
    connect(m_view->verticalScrollBar(), SIGNAL(valueChanged(int)), m_headerContainer->verticalScrollBar(), SLOT(setValue(int)));
    connect(m_headerContainer->verticalScrollBar(), SIGNAL(valueChanged(int)), m_view->verticalScrollBar(), SLOT(setValue(int)));

    connect(pCore->projectManager(), SIGNAL(projectOpened(Project*)), this, SLOT(setProject(Project*)));
}

TimelineWidget::~TimelineWidget()
{
}

void TimelineWidget::setProject(Project* project)
{
    if (m_scene) {
        delete m_scene;
        m_scene = NULL;
    }
    if (project) {
        m_scene = new TimelineScene(project->timeline(), m_toolManager, m_view, this);
    }
    m_view->setScene(m_scene);

    m_headerContainer->setTimeline(project->timeline());
}

TimelineScene* TimelineWidget::scene()
{
    return m_scene;
}

TimelineView* TimelineWidget::view()
{
    return m_view;
}

ToolManager* TimelineWidget::toolManager()
{
    return m_toolManager;
}

#include "timelinewidget.moc"
