/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "monitorview.h"
#include "monitormodel.h"
#include "monitorgraphicsscene.h"
#include "positionbar.h"
#include "widgets/timecodewidget.h"
#include <mlt++/Mlt.h>
#include <KDualAction>
#include <KLocale>
#include <QVBoxLayout>
#include <QGraphicsView>
#include <QGLWidget>
#include <QToolBar>
#include <QAction>

#include <KDebug>


MonitorView::MonitorView(QWidget* parent) :
    QWidget(parent),
    m_model(0)
{
    QVBoxLayout *layout = new QVBoxLayout(this);

    m_videoView = new QGraphicsView(this);
    QGLWidget *glWidget = new QGLWidget();
    m_videoView->setViewport(glWidget);
    m_videoView->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    m_videoView->setFrameShape(QFrame::NoFrame);
    m_videoView->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_videoScene = new MonitorGraphicsScene(this);
    m_videoView->setScene(m_videoScene);
    m_videoView->installEventFilter(m_videoScene);
    m_videoScene->initializeGL(glWidget);
    layout->addWidget(m_videoView, 10);

    m_positionBar = new PositionBar(this);
    layout->addWidget(m_positionBar);

    QToolBar *toolbar = new QToolBar(this);

    m_playAction = new KDualAction(i18n("Play"), i18n("Pause"), this);
    m_playAction->setInactiveIcon(KIcon("media-playback-start"));
    m_playAction->setActiveIcon(KIcon("media-playback-pause"));
    toolbar->addAction(m_playAction);
//     QToolButton *playButton = new QToolButton(toolbar);
//     m_playMenu = new QMenu(i18n("Play..."), this);
//     m_playAction = m_playMenu->addAction(m_playIcon, i18n("Play"));
    //m_playAction->setCheckable(true);
    connect(m_playAction, SIGNAL(triggered()), this, SLOT(togglePlaybackState()));
    layout->addWidget(toolbar);

//     playButton->setMenu(m_playMenu);
//     playButton->setPopupMode(QToolButton::MenuButtonPopup);
//     m_toolbar->addWidget(playButton);

    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);

    m_timecodeWiget = new TimecodeWidget(this);
    toolbar->addWidget(m_timecodeWiget);
}

MonitorView::~MonitorView()
{

}

void MonitorView::setModel(MonitorModel* model)
{
    if (m_model) {
        disconnect(m_model);
        m_model->disconnect(this);
    }
    m_model = model;
    m_videoScene->setFramePointer(m_model->framePointer());
    connect(m_model, SIGNAL(frameUpdated()), m_videoScene, SLOT(update()));
    connect(m_model, SIGNAL(playbackStateChanged(bool)), this, SLOT(onPlaybackStateChange(bool)));
    connect(m_model, SIGNAL(producerChanged()), this, SLOT(onProducerChanged()));

    connect(m_positionBar, SIGNAL(positionChanged(int)), m_model, SLOT(setPosition(int)));
    connect(m_timecodeWiget, SIGNAL(valueChanged(int)), m_model, SLOT(setPosition(int)));
    connect(m_model, SIGNAL(positionChanged(int)), this, SLOT(setPosition(int)));

    connect(m_model, SIGNAL(durationChanged(int)), m_positionBar, SLOT(setDuration(int)));
    connect(m_model, SIGNAL(durationChanged(int)), m_timecodeWiget, SLOT(setMaximum(int)));

    onProducerChanged();
}

MonitorModel* MonitorView::model()
{
    return m_model;
}

void MonitorView::togglePlaybackState()
{
    if (m_model) {
        m_model->togglePlaybackState();
    }
}

void MonitorView::setPosition(int position)
{
    m_positionBar->setPosition(position);
    m_timecodeWiget->setValue(position);
}

void MonitorView::onPlaybackStateChange(bool plays)
{
    m_playAction->setActive(plays);
}

void MonitorView::onProducerChanged()
{
    m_positionBar->setDuration(m_model->duration());
    m_timecodeWiget->setMaximum(m_model->duration());
    setPosition(m_model->position());
}

#include "monitorview.moc"
