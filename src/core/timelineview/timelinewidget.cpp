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
#include "markerswidget.h"
#include "project/timeline.h"
#include "timelinescene.h"
#include "timelinepositionbar.h"
#include "trackheadercontainer.h"
#include "tool/toolmanager.h"
#include "project/project.h"
#include "project/binmodel.h"
#include "project/projectmanager.h"
#include "monitor/monitorview.h"
#include "effectsystem/effectswidget.h"
#include "core.h"
#include <QGridLayout>
#include <QScrollBar>
#include <QStyle>
#include <QSplitter>
#include <QStackedWidget>
#include <KToolBar>
#include <KAction>
#include <KLocale>
#include <KDebug>
#include <KComboBox>


TimelineWidget::TimelineWidget(QWidget* parent) :
    QWidget(parent)
  , m_scene(NULL)
  , m_clipTimeline(NULL)
  , m_monitor(NULL)
{
    QGridLayout *layout = new QGridLayout(this);
    layout->setSpacing(0);
    
    m_toolbar = new KToolBar(this);
    m_toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    m_toolbar->setIconDimensions(style()->pixelMetric(QStyle::PM_SmallIconSize));
    KAction *toolPanelAction = new KAction(KIcon("view-list-tree"), i18n("Show Tool Panel"), this);
    toolPanelAction->setCheckable(true);
    toolPanelAction->setChecked(false);
    m_toolbar->addAction(toolPanelAction);
    layout->addWidget(m_toolbar, 0, 0);


    m_positionBar = new TimelinePositionBar(this);
    layout->addWidget(m_positionBar, 0, 1);

    m_headerContainer = new TrackHeaderContainer(this);
    layout->addWidget(m_headerContainer, 1, 0);
    
    m_splitter = new QSplitter(this);
    layout->addWidget(m_splitter, 1,1);

    m_view = new TimelineView(m_splitter);
    m_splitter->addWidget(m_view);
    
    // Create tool panel for markers, effects, etc
    m_toolContainer = new QFrame(this);
    m_toolContainer->setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
    QGridLayout *l = new QGridLayout;
    m_toolPanelSelector = new KComboBox(m_toolContainer);
    m_toolPanelSelector->addItem(i18n("Effects"));
    m_toolPanelSelector->addItem(i18n("Markers"));
    l->addWidget(m_toolPanelSelector, 0, 0);
    
    KToolBar *toolbar = new KToolBar(this);
    toolbar->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    toolbar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolbar->setIconDimensions(style()->pixelMetric(QStyle::PM_SmallIconSize));
    l->addWidget(toolbar, 0, 1);
    
    
    m_toolPanel = new QStackedWidget(m_toolContainer);
    m_markersWidget = new MarkersWidget(toolbar, this);
    m_effectContainer = new EffectsWidget(toolbar, pCore->effectRepository(), this);
    m_toolPanel->addWidget(m_effectContainer);
    m_effectContainer->fillToolBar();
    m_toolPanel->addWidget(m_markersWidget);
    l->addWidget(m_toolPanel, 1, 0, 1, 2);
    m_toolContainer->setLayout(l);
    m_splitter->addWidget(m_toolContainer);
    //m_toolContainer->setVisible(false);
    m_toolManager = new ToolManager(this);


    connect(m_view, SIGNAL(zoomChanged(int)), m_positionBar, SLOT(setZoomLevel(int)));
    connect(m_view->horizontalScrollBar(), SIGNAL(valueChanged(int)), m_positionBar, SLOT(setOffset(int)));
    connect(toolPanelAction, SIGNAL(toggled(bool)), m_toolContainer, SLOT(setVisible(bool)));
    connect(m_view->horizontalScrollBar(), SIGNAL(rangeChanged(int,int)), m_headerContainer, SLOT(adjustHeight(int,int)));
    connect(m_view->verticalScrollBar(), SIGNAL(valueChanged(int)), m_headerContainer->verticalScrollBar(), SLOT(setValue(int)));
    connect(m_headerContainer->verticalScrollBar(), SIGNAL(valueChanged(int)), m_view->verticalScrollBar(), SLOT(setValue(int)));
    connect(pCore->projectManager(), SIGNAL(projectOpened(Project*)), this, SLOT(setProject(Project*)));
    connect(m_toolPanelSelector, SIGNAL(currentIndexChanged(int)), this, SLOT(slotSwitchToolPanel(int)));
}

TimelineWidget::~TimelineWidget()
{
    delete m_clipTimeline;
}

void TimelineWidget::slotSwitchToolPanel(int ix)
{
    m_toolPanel->setCurrentIndex(ix);
    ToolPanel *panel = static_cast <ToolPanel *>(m_toolPanel->widget(ix));
    panel->fillToolBar();
}

void TimelineWidget::createEffectWidget()
{
    if (m_clipTimeline) {
        m_effectContainer->setTimeline(m_clipTimeline);
    }
}

void TimelineWidget::setProject(Project* project)
{
    delete m_scene;
    m_scene = NULL;
    if (!project) {
        m_view->setScene(NULL);
        return;
    }
    m_scene = new TimelineScene(project->timeline(), m_toolManager, m_view, this);
    m_view->setScene(m_scene);
    m_headerContainer->setTimeline(project->timeline());
    m_monitor = project->timelineMonitor();
    connect(m_monitor, SIGNAL(positionChanged(int,bool)), m_positionBar, SLOT(setCursorPosition(int,bool)));
}

void TimelineWidget::setClipTimeline(Timeline *timeline)
{
    delete m_scene;
    m_scene = NULL;
    if (!timeline) {
        m_view->setScene(NULL);
        return;
    }
    m_clipTimeline = timeline;
    m_positionBar->setProject(timeline->project(), ClipMonitor);
    m_scene = new TimelineScene(timeline, m_toolManager, m_view, this);
    m_view->setScene(m_scene);
    m_headerContainer->setTimeline(timeline);
    m_positionBar->setCursorPosition(timeline->position());
    m_monitor = pCore->projectManager()->current()->binMonitor();
    connect(m_monitor, SIGNAL(positionChanged(int,bool)), m_positionBar, SLOT(setCursorPosition(int,bool)));
    connect(m_markersWidget, SIGNAL(addMarker()), this, SLOT(slotAddMarker()));
    connect(m_markersWidget, SIGNAL(removeMarker(int)), this, SLOT(slotRemoveMarker(int)));
    connect(m_effectContainer, SIGNAL(refreshMonitor()), m_monitor, SLOT(refresh()));
    connect(m_markersWidget, SIGNAL(seek(int)), m_positionBar, SLOT(slotSeek(int)));
    createEffectWidget();
}

TimelineScene* TimelineWidget::scene()
{
    return m_scene;
}

TimelineView* TimelineWidget::view()
{
    return m_view;
}

MonitorView *TimelineWidget::monitor()
{
    return m_monitor;
}

ToolManager* TimelineWidget::toolManager()
{
    return m_toolManager;
}

void TimelineWidget::slotAddMarker()
{
    int markerPosition = m_positionBar->position();
    //TODO: use markercommand for undo / redo
    pCore->projectManager()->current()->bin()->addMarker(property("clipId").toString(), markerPosition);
}

void TimelineWidget::slotRemoveMarker(int pos)
{
    //TODO: use markercommand for undo / redo
    pCore->projectManager()->current()->bin()->removeMarker(property("clipId").toString(), pos);
}

void TimelineWidget::updateMarkers(const QList<int> &markers)
{
    m_markersWidget->setMarkers(markers);
    //m_positionBar->setMarkers(markers);
}

#include "timelinewidget.moc"
