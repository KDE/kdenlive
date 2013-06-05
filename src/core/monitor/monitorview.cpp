/*
Copyright (C) 2013  Jean-Baptiste Mardelle <jb.kdenlive.org>
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


MonitorView::MonitorView(DISPLAYMODE mode, Mlt::Profile *profile, MONITORID id, MONITORID role, QWidget* parent) :
    QWidget(parent)
    , m_id(id)
    , m_currentRole(role)
    , m_profile(new Mlt::Profile(profile->get_profile()))
    , m_seekPosition(SEEK_INACTIVE)
    , m_monitorProducer(NULL)
    , m_infoMessage(NULL)
{
    m_layout = new QGridLayout(this);
    m_layout->setVerticalSpacing(0);
    m_blackProducer = new ProducerWrapper(*m_profile, "blipflash");//color:black");
    m_blackProducer->set_in_and_out(0, 1);
    m_monitorProducer= m_blackProducer;
    
    // Setup monitor modes
    m_monitorMode = new KSelectAction(this);
    KAction *monitorType;
    QList <DISPLAYMODE> modes = pCore->monitorManager()->supportedDisplayModes();
    for(int i = 0; i < modes.count(); ++i) {
	monitorType = m_monitorMode->addAction(pCore->monitorManager()->getDisplayName(modes.at(i)));
	monitorType->setData(modes.at(i));
	if (mode == modes.at(i)) m_monitorMode->setCurrentAction(monitorType);
    }
    
    m_infoMessage = new KMessageWidget(this);
    m_infoMessage->setMessageType(KMessageWidget::Warning);
    m_infoMessage->setCloseButtonVisible(true);
    m_infoMessage->setWordWrap(true);
    m_layout->addWidget(m_infoMessage, 1, 0, 1, 1);
    m_infoMessage->hide();
    
    if (m_monitorMode->actions().isEmpty()) {
	m_infoMessage->setText(i18n("Your MLT config does not support OpenGL nor SDL, Kdenlive will not be usable"));
	m_infoMessage->show();
	//TODO: build dummy controller
    }
    else {
	if (pCore->monitorManager()->isAvailable(mode)) {
	    m_controller = buildController(mode);
	}
    }
    m_layout->addWidget(m_controller->displayWidget(), 0, 0, 1, 1);

    //m_layout->addWidget(m_scenecontroller->displayWidget(), 1, 0, 1 , 1);
    //m_scenecontroller->displayWidget()->setHidden(true);

    m_audioSignal = new AudioSignal(this);
    m_layout->addWidget(m_audioSignal, 0, 1, 1, 1);
    m_audioSignal->setHidden(true);
    
    m_positionBar = new PositionBar(this);
    m_layout->addWidget(m_positionBar, 2, 0, 1, -1);

    m_toolbar = new QToolBar(this);

    m_playAction = new KDualAction(i18n("Play"), i18n("Pause"), this);
    m_playAction->setInactiveIcon(KIcon("media-playback-start"));
    m_playAction->setActiveIcon(KIcon("media-playback-pause"));
    m_toolbar->addAction(m_playAction);
    m_toolbar->addAction(m_monitorMode);
    
    m_monitorRole = new KSelectAction(this);
    KAction *mRole = m_monitorRole->addAction(nameForRole(role));
    mRole->setData(role);
    m_monitorRole->setCurrentAction(mRole);
    if (id == AutoMonitor || id == ClipMonitor) {
	mRole = m_monitorRole->addAction(nameForRole(RecordMonitor));
	mRole->setData(RecordMonitor);
    }
    m_toolbar->addAction(m_monitorRole);
    m_monitorRole->setVisible(m_monitorRole->actions().count() > 1);
    
    /*m_sceneAction = new KAction(KIcon("video-display"), i18n("Scene Mode"), this);
    m_sceneAction->setCheckable(true);
    m_sceneAction->setChecked(false);
    m_toolbar->addAction(m_sceneAction);*/
    
    KAction *audioAction = new KAction(KIcon("player-volume"), i18n("Monitor Audio"), this);
    audioAction->setCheckable(true);
    audioAction->setChecked(false);
    m_toolbar->addAction(audioAction);
    
    KAction *zoneAction = new KAction(KIcon("go-top"), i18n("Use Zone"), this);
    zoneAction->setCheckable(true);
    zoneAction->setChecked(false);
    m_toolbar->addAction(zoneAction);
    
    m_layout->addWidget(m_toolbar, 3, 0, 1, -1);

//     playButton->setMenu(m_playMenu);
//     playButton->setPopupMode(QToolButton::MenuButtonPopup);
//     m_toolbar->addWidget(playButton);

    QWidget *spacer = new QWidget(this);
    spacer->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    m_toolbar->addWidget(spacer);

    m_timecodeWiget = new TimecodeWidget(this);
    m_toolbar->addWidget(m_timecodeWiget);
    
    connect(m_playAction, SIGNAL(triggered()), this, SLOT(checkPlaybackState()));
    //connect(m_sceneAction, SIGNAL(triggered()), this, SLOT(toggleSceneState()));
    connect(m_monitorMode, SIGNAL(triggered(QAction*)), this, SLOT(slotToggleMonitorMode(QAction*)));
    connect(m_monitorRole, SIGNAL(triggered(QAction*)), this, SLOT(slotToggleMonitorRole(QAction*)));
    connect(audioAction, SIGNAL(triggered(bool)), this, SLOT(toggleAudioMonitor(bool)));
    connect(zoneAction, SIGNAL(triggered(bool)), this, SLOT(toggleZone(bool)));
    connect(m_positionBar, SIGNAL(positionChanged(int)), this, SLOT(seek(int)));
    connect(m_timecodeWiget, SIGNAL(valueChanged(int)), this, SLOT(seek(int)));
    connect(m_positionBar, SIGNAL(requestThumb(int)), this, SLOT(slotGetThumb(int)));
    connectController();
    
}

MltController *MonitorView::buildController(DISPLAYMODE mode)
{
    MltController *cont = NULL;
    switch (mode) {
      case MLTOPENGL:
	  cont = new GLWidget(m_profile, this);
	  break;
      case MLTGLSL:
	  cont = new GLSLWidget(m_profile, this);
	  break;
      case MLTSCENE:
	  cont = new SceneWidget(m_profile, this);
	  break;
      default:
	  cont = new SDLWidget(m_profile, this);
	  break;
    }
    return cont;
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

void MonitorView::slotFocusClipMonitor()
{
    if (m_currentRole == ClipMonitor) return;
    toggleMonitorRole(ClipMonitor);
}

void MonitorView::setProfile(Mlt::Profile *profile, bool reset)
{
    if (!requiresProfileUpdate(m_profile, profile)) return;
    //double speed = m_monitorProducer? m_monitorProducer->get_speed(): 0;
    //int position = m_monitorProducer? m_monitorProducer->position() : 0;
    m_controller->setProfile(profile);
    m_controller->close();
    m_profile->set_colorspace(profile->colorspace());
    m_profile->set_frame_rate(profile->frame_rate_num(), profile->frame_rate_den());
    m_profile->set_height(profile->height());
    m_profile->set_progressive(profile->progressive());
    m_profile->set_sample_aspect(profile->sample_aspect_num(), profile->sample_aspect_den());
    m_profile->set_width(profile->width());
    m_profile->get_profile()->display_aspect_num = profile->display_aspect_num();
    m_profile->get_profile()->display_aspect_den = profile->display_aspect_den();
    m_profile->set_explicit(1);
    delete m_blackProducer;
    m_blackProducer = new ProducerWrapper(*profile, "blipflash");
    m_blackProducer->set_in_and_out(0, 1);
    if (reset) {
	m_monitorProducer = m_blackProducer;
	open(m_blackProducer);
    }
    else m_monitorProducer = NULL;
    //seek(position);
    //m_controller->play(speed);
    return;
  
    m_controller->closeConsumer();
    //m_normalProducer = NULL;
    QList <MONITORID> roles;
    QHash<MONITORID, ProducerWrapper*>::iterator i = m_GPUProducer.begin();
    while (i != m_GPUProducer.end()) {
	roles.append(i.key());
	ProducerWrapper *p = m_GPUProducer.take(i.key());
	delete p;
	i = m_GPUProducer.begin();
    }
 
    delete m_blackProducer;
    m_blackProducer = new ProducerWrapper(*profile, "blipflash"); //color:black");
    m_normalProducer.insert(m_currentRole, m_blackProducer);
    m_monitorProducer = m_normalProducer.value(m_currentRole);
  
    /*bool wasBlack = m_monitorProducer->resourceName() == m_blackProducer->resourceName();
    m_controller->closeConsumer();
    ProducerWrapper *newBlack = new ProducerWrapper(*profile, "blipflash"); //color:black");
    newBlack->set_in_and_out(0, 1);
    delete m_blackProducer;
    m_blackProducer = newBlack;
    if (wasBlack) {
	//m_controller->open(newBlack);
	m_monitorProducer = m_blackProducer;
	m_normalProducer = m_blackProducer;
    }
    else {
	delete m_normalProducer;
	delete m_GPUProducer;
	m_normalProducer = NULL;
	m_GPUProducer = NULL;
	m_monitorProducer = NULL;
    }*/
      
    delete m_profile;
    m_profile = profile;
    toggleMonitorMode((DISPLAYMODE) m_monitorMode->currentAction()->data().toInt(), false);
    //if (wasBlack) m_blackProducer = m_normalProducer;
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

const QString MonitorView::nameForRole(MONITORID role)
{
    QString roleName;
    switch (role) {
      case ProjectMonitor:
	  roleName = i18n("Project Monitor");
	  break;
      case RecordMonitor:
	  roleName = i18n("Record Monitor");
	  break;
      default:
	  roleName = i18n("Clip Monitor");
	  break;
	  
    }
    return roleName;
}

void MonitorView::addMonitorRole(MONITORID role)
{
    QList <QAction *> actions = m_monitorRole->actions();
    for (int i = 0; i < actions.count(); i++)
	if ((MONITORID) (actions.at(i)->data().toInt()) ==  role) return;
    KAction *mRole = m_monitorRole->addAction(nameForRole(role));    
    mRole->setData(role);
    m_monitorRole->setVisible(m_monitorRole->actions().count() > 1);
}

void MonitorView::seek(int pos, MONITORID role)
{
    toggleMonitorRole(role);
    if (role == RecordMonitor) {
	// No seeking allowed on live sources
	return;
    }
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

void MonitorView::toggleAudioMonitor(bool active)
{
    if (true) {
	pCore->monitorManager()->requestActivation(m_id);
	//ProducerWrapper *tester = new ProducerWrapper(m_controller->profile(), QString("/tmp/00110.MTS"));
	//open(tester);
    }
    if (active) {
	connect(this, SIGNAL(audioLevels(QVector<double>)), m_audioSignal, SLOT(slotAudioLevels(QVector<double>)));
	m_audioSignal->setHidden(false);
    }
    else {
	disconnect(this, SIGNAL(audioLevels(QVector<double>)), m_audioSignal, SLOT(slotAudioLevels(QVector<double>)));
	m_audioSignal->setHidden(true);
    }
}

void MonitorView::toggleZone(bool active)
{
    if (active) {
	int inPoint = m_positionBar->position();
	if (inPoint >= m_positionBar->duration() - 5) inPoint = 0;
	int outPoint = (m_positionBar->duration() - 1);
	if (outPoint > 50) outPoint = outPoint / 5;
	m_positionBar->setZone(QPoint(inPoint, inPoint + outPoint));
    }
    else {
	m_positionBar->setZone(QPoint());
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

int MonitorView::open(ProducerWrapper* producer, MONITORID role, bool isMulti)
{
    //kDebug()<<"MONITOR: "<<m_id<<"\n + + + ++ + + + + + +\nOPENING PRODUCeR: "<<producer->resourceName()<<" = "<<producer->get_length()<<" \n + + + + ++ + + + + ++ + +\n";
    setProfile(producer->profile(), false);
    if (role != KeepMonitor && role != m_currentRole) {
	// Switching monitor role
	m_currentRole = role;
	addMonitorRole(role);
    }
    if (m_monitorProducer == producer) return 0;
    if (m_GPUProducer.contains(m_currentRole)) {
	if (m_controller->displayType() == MLTGLSL) m_normalProducer.value(m_currentRole)->seek(m_GPUProducer.value(m_currentRole)->position());
	m_controller->stop();
	delete m_GPUProducer.take(m_currentRole);
    }
    if (producer != m_blackProducer) m_normalProducer.insert(m_currentRole, producer);
    if (m_controller->displayType() == MLTGLSL) {
	QString resource = producer->resourceName();
	ProducerWrapper *GPUProducer = new ProducerWrapper(*m_profile, resource);
	GPUProducer->seek(producer->position());
	if (producer != m_blackProducer) m_GPUProducer.insert(m_currentRole, GPUProducer);
	m_monitorProducer = GPUProducer;
    }
    else {
	m_monitorProducer = producer;
    }
    int result = m_controller->open(m_monitorProducer, isMulti, m_currentRole == RecordMonitor);
    return result;
}


bool MonitorView::requiresProfileUpdate(Mlt::Profile *base, Mlt::Profile *newProfile) const
{
    if (newProfile->width() == base->width() && newProfile->height() == base->height() && newProfile->dar() == base->dar() && newProfile->progressive() == base->progressive())
	return false;
    return true;
}

DISPLAYMODE MonitorView::displayType() const
{
    return m_controller->displayType();
}

void MonitorView::slotToggleMonitorRole(QAction *a)
{
    MONITORID mode = (MONITORID) (a->data().toInt());
    toggleMonitorRole(mode, false);
}

void MonitorView::toggleMonitorRole(MONITORID role, bool setAction)
{
    if (role != KeepMonitor && role != m_currentRole) {
	// switch to required role
	if (setAction) {
	    QList <QAction *> actions = m_monitorRole->actions();
	    for (int i = 0; i < actions.count(); i++)
		if ((MONITORID) (actions.at(i)->data().toInt()) ==  role) {
		    m_monitorRole->setCurrentAction(actions.at(i));
		    break;
		}
	}
	if (m_currentRole == RecordMonitor) {
	    m_normalProducer.remove(RecordMonitor);
	    m_GPUProducer.remove(RecordMonitor);
	    m_controller->closeConsumer();
	}
	m_currentRole = role;
	addMonitorRole(role);
	if (m_controller->displayType() == MLTGLSL) {
	    if (m_GPUProducer.contains(m_currentRole)) m_monitorProducer = m_GPUProducer.value(m_currentRole);
	    else if (m_normalProducer.contains(m_currentRole)) {
		ProducerWrapper *GPUProducer = new ProducerWrapper(*m_profile, m_normalProducer.value(m_currentRole)->get("resource"));
		if (role != RecordMonitor) GPUProducer->seek(m_monitorProducer->position());
		m_GPUProducer.insert(m_currentRole, GPUProducer);
		m_monitorProducer = GPUProducer;
	    }
	    else {
		if (role == RecordMonitor) {
		    m_monitorProducer = new ProducerWrapper(*m_profile, "decklink:");
		    m_normalProducer.insert(role, m_monitorProducer);
		}
		else m_monitorProducer = m_blackProducer;
	    }
	}
	else {
	    if (m_normalProducer.contains(m_currentRole)) m_monitorProducer = m_normalProducer.value(m_currentRole);
	    else {
		if (role == RecordMonitor) {
		    m_monitorProducer = new ProducerWrapper(*m_profile, "decklink:");
		    m_normalProducer.insert(role, m_monitorProducer);
		}
		else m_monitorProducer = m_blackProducer;
	    }
	}
	m_positionBar->setVisible(role != RecordMonitor);
	m_playAction->setVisible(role != RecordMonitor);
	m_controller->open(m_monitorProducer, false, m_currentRole == RecordMonitor);
    }
}

void MonitorView::slotToggleMonitorMode(QAction *a)
{
    DISPLAYMODE mode = (DISPLAYMODE) (a->data().toInt());
    toggleMonitorMode(mode);
}

void MonitorView::toggleMonitorMode(DISPLAYMODE mode, bool checkAvailability)
{
    if (checkAvailability) {
	m_infoMessage->hide();
	if (!pCore->monitorManager()->isSupported((DISPLAYMODE) mode)) {
	    m_infoMessage->setText(i18n("The video mode <b>%1</b> is not available on your system", pCore->monitorManager()->getDisplayName(mode)));
	    m_infoMessage->show();
	    m_monitorMode->setCurrentItem(m_controller->displayType());
	    return;
	}
	if (!pCore->monitorManager()->isAvailable((DISPLAYMODE) mode)) {
	    m_infoMessage->setText(i18n("Only one <b>%1</b> monitor can be used", pCore->monitorManager()->getDisplayName(mode)));
	    m_infoMessage->show();
	    m_monitorMode->setCurrentItem(m_controller->displayType());
	    return;
	}
    }
    setUpdatesEnabled(false);
    int pos = m_controller->position();
    m_controller->stop();
    QString resource = m_monitorProducer->resourceName();
    DISPLAYMODE previousMode = m_controller->displayType();
    m_controller->closeConsumer();
    //m_controller->close();
    connectController(false);
    m_layout->removeWidget(m_controller->displayWidget());
    delete m_controller;
    m_controller = buildController(mode);

    if (previousMode != MLTGLSL && mode == MLTGLSL) {
	// Switching to GPU acclerated
	// Special case: glsl uses different normalizers, we must recreate the producer
	if (!m_GPUProducer.contains(m_currentRole)) {
	    ProducerWrapper *GPUProducer = new ProducerWrapper(*m_profile, resource);
	    GPUProducer->seek(m_monitorProducer->position());
	    m_GPUProducer.insert(m_currentRole, GPUProducer);
	    //m_normalProducer = m_monitorProducer;
	}
	m_monitorProducer = m_GPUProducer.value(m_currentRole);
    }
    if (previousMode == MLTGLSL && mode != MLTGLSL) {
	// Switching back to normal producer
	// Special case: glsl uses different normalizers, we must recreate the producer
	if (m_normalProducer.contains(m_currentRole)) m_monitorProducer = m_normalProducer.value(m_currentRole);
	else m_monitorProducer = m_blackProducer;
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
    if (m_currentRole == ProjectMonitor) emit positionChanged(position, m_seekPosition != SEEK_INACTIVE);
}

void MonitorView::onProducerChanged()
{
    int producerDuration = 0;
    if (m_monitorProducer) {// controller->producer()) 
	producerDuration = m_monitorProducer->get_length();
    }
    m_positionBar->setDuration(producerDuration, m_profile->fps());
    m_timecodeWiget->setMaximum(producerDuration);
    setPosition(m_monitorProducer->position());
    m_playAction->setActive(false);
}

MltController *MonitorView::controller()
{
    return m_controller;
}

#include "monitorview.moc"
