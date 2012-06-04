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
#include <mlt++/Mlt.h>
#include <KLocale>
#include <QVBoxLayout>
#include <QGraphicsView>
#include <QGLWidget>
#include <QToolBar>
#include <QAction>


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

    m_playIcon = KIcon("media-playback-start");
    m_pauseIcon = KIcon("media-playback-pause");
    m_playAction = toolbar->addAction(m_playIcon, i18n("Play..."));
//     QToolButton *playButton = new QToolButton(toolbar);
//     m_playMenu = new QMenu(i18n("Play..."), this);
//     m_playAction = m_playMenu->addAction(m_playIcon, i18n("Play"));
    //m_playAction->setCheckable(true);
    connect(m_playAction, SIGNAL(triggered()), this, SLOT(togglePlaybackState()));
    layout->addWidget(toolbar);

//     playButton->setMenu(m_playMenu);
//     playButton->setPopupMode(QToolButton::MenuButtonPopup);
//     m_toolbar->addWidget(playButton);
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

    m_positionBar->setDuration(m_model->duration());
    m_positionBar->setPosition(m_model->position());
    connect(m_positionBar, SIGNAL(positionChanged(int)), m_model, SLOT(setPosition(int)));
    connect(m_model, SIGNAL(positionChanged(int)), m_positionBar, SLOT(setPosition(int)));
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

void MonitorView::onPlaybackStateChange(bool plays)
{
    m_playAction->setIcon(plays ? m_pauseIcon : m_playIcon);
}


#include "monitorview.moc"
