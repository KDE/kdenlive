/*
Copyright (C) 2012  Till Theato <root@ttill.de>
This file is part of kdenlive. See www.kdenlive.org.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.
*/

#include "monitorview.h"

#include "monitorgraphicsscene.h"
#include "monitormanager.h"
#include "positionbar.h"
#include "audiosignal.h"
#include "glwidget.h"
#include "glslwidget.h"
#include "scenewidget.h"
#include "sdlwidget.h"
#include "core.h"
#include "project/producerwrapper.h"
#include "widgets/timecodewidget.h"
#include <mlt++/Mlt.h>
#include <KDualAction>
#include <KLocale>
#include <KSelectAction>
#include <KMessageWidget>
#include <KUrl>
#include <QVBoxLayout>
#include <QGraphicsView>
#include <QGLWidget>
#include <QToolBar>
#include <QAction>

#include <KDebug>

#define SEEK_INACTIVE (-1)


MonitorView::MonitorView(Mlt::Profile *profile, MONITORID id, QWidget* parent) :
    QWidget(parent)
    , m_id(id)
    , m_profile(profile)
    , m_seekPosition(SEEK_INACTIVE)
    , m_monitorProducer(NULL)
    , m_infoMessage(NULL)
{
    m_layout = new QGridLayout(this);
    m_blackProducer = new ProducerWrapper(*profile, "color:black");
    m_blackProducer->set_in_and_out(0, 1);
    m_monitorProducer = m_blackProducer;
    if (pCore->monitorManager()->isAvailable(MLTOPENGL)) {
	m_controller = new GLWidget(m_profile, this);
    }
    else  {
	m_controller = new SDLWidget(m_profile, this);
    }
    m_layout->addWidget(m_controller->displayWidget(), 0, 0, 1, 1);
    
    m_infoMessage = new KMessageWidget(this);
    m_infoMessage->setMessageType(KMessageWidget::Warning);
    m_infoMessage->setCloseButtonVisible(true);
    m_infoMessage->setWordWrap(true);
    m_layout->addWidget(m_infoMessage, 1, 0, 1, 1);
    m_infoMessage->hide();
    if (m_controller->displayType() == MLTSDL && !pCore->monitorManager()->isAvailable(m_controller->displayType())) {
	m_infoMessage->setText(i18n("Your MLT config does not support OpenGL nor SDL, Kdenlive will not be usable"));
	m_infoMessage->show();
    }
    //m_layout->addWidget(m_scenecontroller->displayWidget(), 1, 0, 1 , 1);
    //m_scenecontroller->displayWidget()->setHidden(true);

    m_audioSignal = new AudioSignal(this);
    m_layout->addWidget(m_audioSignal, 0, 1, 1, 1);
    m_audioSignal->setHidden(true);
    
    m_positionBar = new PositionBar(this);
    m_layout->addWidget(m_positionBar, 2, 0, 1, -1);

    QToolBar *toolbar = new QToolBar(this);

    m_playAction = new KDualAction(i18n("Play"), i18n("Pause"), this);
    m_playAction->setInactiveIcon(KIcon("media-playback-start"));
    m_playAction->setActiveIcon(KIcon("media-playback-pause"));
    toolbar->addAction(m_playAction);
    
    m_monitorMode = new KSelectAction(this);
    m_monitorMode->setItems(QStringList() << "OpenGL" << "GLSL" << "Scene Mode" << "SDL");
    m_monitorMode->setCurrentItem(m_controller->displayType());
    toolbar->addAction(m_monitorMode);
    
    /*m_sceneAction = new KAction(KIcon("video-display"), i18n("Scene Mode"), this);
    m_sceneAction->setCheckable(true);
    m_sceneAction->setChecked(false);
    toolbar->addAction(m_sceneAction);*/

    m_audioAction = new KAction(KIcon("player-volume"), i18n("Monitor Audio"), this);
    m_audioAction->setCheckable(true);
    m_audioAction->setChecked(false);
    toolbar->addAction(m_audioAction);
//     QToolButton *playButton = new QToolButton(toolbar);
//     m_playMenu = new QMenu(i18n("Play..."), this);
//     m_playAction = m_playMenu->addAction(m_playIcon, i18n("Play"));
    //m_playAction->setCheckable(true);
    m_layout->addWidget(toolbar, 3, 0, 1, -1);

//     playButton->setMenu(m_playMenu);
//     playButton->setPopupMode(QToolButton::MenuButtonPopup);
//     m_toolbar->addWidget(playButton);

    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    toolbar->addWidget(spacer);

    m_timecodeWiget = new TimecodeWidget(this);
    toolbar->addWidget(m_timecodeWiget);
    
    connect(m_playAction, SIGNAL(triggered()), this, SLOT(checkPlaybackState()));
    //connect(m_sceneAction, SIGNAL(triggered()), this, SLOT(toggleSceneState()));
    connect(m_monitorMode, SIGNAL(triggered(int)), this, SLOT(toggleMonitorMode(int)));
    connect(m_audioAction, SIGNAL(triggered()), this, SLOT(toggleAudioMonitor()));
    connect(m_positionBar, SIGNAL(positionChanged(int)), this, SLOT(seek(int)));
    connect(m_timecodeWiget, SIGNAL(valueChanged(int)), this, SLOT(seek(int)));
    connect(m_positionBar, SIGNAL(requestThumb(int)), this, SLOT(slotGetThumb(int)));
    connectController();
    
}

MONITORID MonitorView::id() const
{
    return m_id;
}

void MonitorView::slotGetThumb(int pos)
{
    emit requestThumb(m_monitorProducer, pos);
}

void MonitorView::refresh()
{
    m_controller->refreshConsumer();
}

int MonitorView::position() const
{
    return m_controller->position();
}

void MonitorView::setProfile(Mlt::Profile *profile)
{
    m_profile = profile;
    bool wasBlack = m_monitorProducer == m_blackProducer;
    delete m_blackProducer;
    m_blackProducer = new ProducerWrapper(*profile, "color:black");
    m_blackProducer->set_in_and_out(0, 1);
    if (wasBlack) m_monitorProducer = m_blackProducer;
    toggleMonitorMode(m_monitorMode->currentItem(), false);
}

void MonitorView::connectController(bool doConnect)
{
    if (doConnect) {
        connect(m_controller->videoWidget(), SIGNAL(producerChanged()), this, SLOT(onProducerChanged()));
	connect(m_controller->videoWidget(), SIGNAL(frameReceived(Mlt::QFrame)), this, SLOT(onShowFrame(Mlt::QFrame)));
	connect(m_controller->videoWidget(), SIGNAL(gotThumb(int, QImage)), m_positionBar, SLOT(slotSetThumbnail(int, QImage)));
	connect(m_controller->videoWidget(), SIGNAL(requestPlayPause()), this, SLOT(togglePlaybackState()));
	connect(m_controller->videoWidget(), SIGNAL(seekTo(int)), this, SLOT(seek(int)));
	connect(m_controller->videoWidget(), SIGNAL(stateChanged()), this, SLOT(slotCheckPlayState()));
	connect(this, SIGNAL(requestThumb(ProducerWrapper *, int)), m_controller->videoWidget(), SLOT(slotGetThumb(ProducerWrapper *, int)));
    }
    else {
	disconnect(m_controller->videoWidget(), SIGNAL(producerChanged()), this, SLOT(onProducerChanged()));
	disconnect(m_controller->videoWidget(), SIGNAL(frameReceived(Mlt::QFrame)), this, SLOT(onShowFrame(Mlt::QFrame)));
	disconnect(m_controller->videoWidget(), SIGNAL(gotThumb(int, QImage)), m_positionBar, SLOT(slotSetThumbnail(int, QImage)));
	disconnect(m_controller->videoWidget(), SIGNAL(requestPlayPause()), this, SLOT(togglePlaybackState()));
	disconnect(m_controller->videoWidget(), SIGNAL(seekTo(int)), this, SLOT(seek(int)));
	disconnect(m_controller->videoWidget(), SIGNAL(stateChanged()), this, SLOT(slotCheckPlayState()));
	disconnect(this, SIGNAL(requestThumb(ProducerWrapper *, int)), m_controller->videoWidget(), SLOT(slotGetThumb(ProducerWrapper *, int)));
    }
}

MonitorView::~MonitorView()
{
    delete m_controller;
}

void MonitorView::slotCheckPlayState()
{
    if (!m_monitorProducer) m_playAction->setActive(false);
    else m_playAction->setActive(m_monitorProducer->get_speed() != 0);
}

void MonitorView::onShowFrame(Mlt::QFrame frame)
{
    int position = frame.frame()->get_position();
    m_controller->displayPosition = position;
    setPosition(position);
    if (m_seekPosition != SEEK_INACTIVE)
        m_controller->seek(m_seekPosition);
    m_seekPosition = SEEK_INACTIVE;
    showAudio(frame.frame());
}

void MonitorView::showAudio(Mlt::Frame* frame)
{
    if (frame->get_int("test_audio"))
        return;
    QVector<double> channels;
    int n = frame->get_int("audio_channels");
    while (n--) {
        QString s = QString("meta.media.audio_level.%1").arg(n);
        channels << frame->get_double(s.toAscii().constData());
    }
    emit audioLevels(channels);
}

void MonitorView::seek(int pos)
{
    if (pos >= 0) {
	if (m_seekPosition == SEEK_INACTIVE)
	      m_controller->seek(pos);
	m_seekPosition = pos;
	m_positionBar->setSeekPos(pos);
    }
}

void MonitorView::togglePlaybackState()
{
    /*if (!m_controller->isActive()) pCore->monitorManager()->requestActivation(m_id);
    if (m_playAction->isActive()) m_controller->play(1.0);
    else m_controller->pause();*/
    m_playAction->setActive(!m_playAction->isActive());
    checkPlaybackState();
}

void MonitorView::checkPlaybackState()
{
    if (!m_controller->isActive()) {
	pCore->monitorManager()->requestActivation(m_id);
	//m_controller->reOpen();
    }
    if (m_playAction->isActive()) m_controller->play(1.0);
    else m_controller->pause();
}

void MonitorView::toggleAudioMonitor()
{
    if (m_monitorProducer == m_blackProducer) {
	pCore->monitorManager()->requestActivation(m_id);
	ProducerWrapper *tester = new ProducerWrapper(m_controller->profile(), QString("/home/two/Videos/gramme/00110.MTS"));
	open(tester);
    }
    if (m_audioAction->isChecked()) {
	connect(this, SIGNAL(audioLevels(QVector<double>)), m_audioSignal, SLOT(slotAudioLevels(QVector<double>)));
	m_audioSignal->setHidden(false);
    }
    else {
	disconnect(this, SIGNAL(audioLevels(QVector<double>)), m_audioSignal, SLOT(slotAudioLevels(QVector<double>)));
	m_audioSignal->setHidden(true);
    }
}
/*
void MonitorView::toggleSceneState()
{
    int pos = m_controller->position();
    //m_controller->stop();
    m_controller->closeConsumer();
    //m_controller->close();
    connectController(false);
    if (m_sceneAction->isChecked()) {
	// Switch to graphicsscene controller
	m_controller = m_scenecontroller;
	if (m_glcontroller->usesGpu()) {
	    ProducerWrapper *prod2 = new ProducerWrapper(m_controller->profile(), m_glcontroller->resource());
	    m_controller->open(prod2);
	}
	else if (m_glcontroller->resource() != m_scenecontroller->resource()) {
	    m_controller->open(m_glcontroller->producer());
	}
	else m_controller->reOpen();
	m_controller->seek(pos);
	m_glcontroller->displayWidget()->setHidden(true);
	m_scenecontroller->displayWidget()->setHidden(false);
    }
    else {
	// Switch to OpenGL controller
	m_controller = m_glcontroller;
	if (m_glcontroller->usesGpu()) {
	    if (m_glcontroller->resource() == m_scenecontroller->resource()) m_controller->reOpen();
	    else {
		ProducerWrapper *prod2 = new ProducerWrapper(m_controller->profile(), m_scenecontroller->resource());
		m_controller->open(prod2);
	    }
	}
	else if (m_glcontroller->resource() != m_scenecontroller->resource()) {
	    kDebug()<<"// GL WIDGET RE-OPENING";
	    m_controller->open(m_scenecontroller->producer());
	}
	else m_controller->reOpen();
	m_controller->seek(pos);
	m_scenecontroller->displayWidget()->setHidden(true);
	m_glcontroller->displayWidget()->setHidden(false);
    }
    connectController();
    emit controllerChanged(m_id, m_controller);
    slotCheckPlayState();
    m_controller->reStart();
    //m_controller->pause();
}
*/

int MonitorView::open(ProducerWrapper* producer, bool isMulti)
{
    m_monitorProducer = producer;
    return m_controller->open(producer, isMulti);
}

DISPLAYMODE MonitorView::displayType() const
{
    return m_controller->displayType();
}

void MonitorView::toggleMonitorMode(int mode, bool checkAvailability)
{
    if (checkAvailability) {
	m_infoMessage->hide();
	if (!pCore->monitorManager()->isSupported((DISPLAYMODE) mode)) {
	    QString message;
	    switch (mode) {
	      case MLTSDL:
		  message = "SDL";
		  break;
	      case MLTGLSL:
		  message = "GPU accelerated GLSL";
		  break;
	      case MLTOPENGL:
	      case MLTSCENE:
		  message = "OpenGL";
		  break;
	      default:
		  message = "unknown";
	    }
	    m_infoMessage->setText(i18n("The video mode <b>%1</b> is not available on your system", message));
	    m_infoMessage->show();
	    m_monitorMode->setCurrentItem(m_controller->displayType());
	    return;
	}
	if (!pCore->monitorManager()->isAvailable((DISPLAYMODE) mode)) {
	    QString message;
	    switch (mode) {
	      case MLTSDL:
		  message = "SDL";
		  break;
	      case MLTGLSL:
		  message = "GPU accelerated GLSL";
		  break;
	      case MLTOPENGL:
	      case MLTSCENE:
		  message = "OpenGL";
		  break;
	      default:
		  message = "unknown";
	    }
	    m_infoMessage->setText(i18n("Only one <b>%1</b> monitor can be used", message));
	    m_infoMessage->show();
	    m_monitorMode->setCurrentItem(m_controller->displayType());
	    return;
	}
    }
    setUpdatesEnabled(false);
    int pos = m_controller->position();
    m_controller->stop();
    QString resource;
    if (m_monitorProducer && (m_controller->displayType() == MLTGLSL || mode == MLTGLSL)) {
	// Special case: glsl uses different normalizers, we must recreate the producer
	resource = m_monitorProducer->resourceName();
	delete m_monitorProducer;
    }
    m_controller->closeConsumer();
    //m_controller->close();
    connectController(false);
    m_layout->removeWidget(m_controller->displayWidget());
    delete m_controller;
    if (mode == 2) {
	// Switch to graphicsscene controller
	m_controller = new SceneWidget(m_profile, this);
	if (!resource.isEmpty()) {
	    // Special case: glsl uses different normalizers, we must recreate the producer
	    ProducerWrapper *prod = new ProducerWrapper(*m_profile, resource);
	    m_monitorProducer = prod;
	}
    }
    else if (mode == 3) {
	m_controller = new SDLWidget(m_profile, this);
	if (!resource.isEmpty()) {
	    // Special case: glsl uses different normalizers, we must recreate the producer
	    ProducerWrapper *prod = new ProducerWrapper(*m_profile, resource);
	    m_monitorProducer = prod;
	}
    }
    else if (mode == 0) {
	m_controller = new GLWidget(m_profile, this);
	if (!resource.isEmpty()) {
	    // Special case: glsl uses different normalizers, we must recreate the producer
	    ProducerWrapper *prod = new ProducerWrapper(*m_profile, resource);
	    m_monitorProducer = prod;
	}
    } else if (mode == 1) {
	m_controller = new GLSLWidget(m_profile, this);
	ProducerWrapper *prod = new ProducerWrapper(*m_profile, resource);
	m_monitorProducer = prod;
    }
    connectController();
    emit controllerChanged(m_id, m_controller);
    m_layout->addWidget(m_controller->displayWidget(), 0, 0, 1, 1);
    setUpdatesEnabled(true);
    m_controller->open(m_monitorProducer);
    m_controller->seek(pos);
    slotCheckPlayState();
    m_controller->reStart();
}

bool MonitorView::isActive() const
{
    return m_controller->isActive();
}

void MonitorView::activate()
{
    m_controller->activate();
}

void MonitorView::close()
{
    m_controller->close();
    m_controller->closeConsumer();
}

void MonitorView::setPosition(int position)
{
    if (position >= m_positionBar->duration() - 1) {
	m_controller->pause();
	m_playAction->setActive(false);
    }
    m_positionBar->setPosition(position);
    m_timecodeWiget->setValue(position);
}

void MonitorView::onProducerChanged()
{
    int producerDuration = 0;
    if (m_controller->producer()) producerDuration = m_controller->producer()->get_length();
    m_positionBar->setDuration(producerDuration);
    m_timecodeWiget->setMaximum(producerDuration);
    setPosition(m_controller->position());
    m_playAction->setActive(false);
}

MltController *MonitorView::controller()
{
    return m_controller;
}

#include "monitorview.moc"
