/***************************************************************************
                          kmmscreen.cpp  -  description
                             -------------------
    begin                : Mon Mar 25 2002
    copyright            : (C) 2002 by Jason Wood
    email                : jasonwood@blueyonder.co.uk
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "kmmscreen.h"
#include <string.h>
#include <iostream>
#include <kdebug.h>
#include <klocale.h>

#include "krender.h"
#include "krendermanager.h"
#include "kdenlive.h"
#include "kdenlivedoc.h"

namespace Gui {

    KMMScreen::KMMScreen(KdenliveApp * app, QWidget * parent,
	const char *name):QVBox(parent, name),
        m_render(app->renderManager()->findRenderer(name)), m_app(app),
        m_clipLength(0), m_name(name)
    {
	//

	//connect(m_render, SIGNAL(replyCreateVideoXWindow(WId)), this, SLOT(embedWindow(WId)));
	/*connect(m_render, SIGNAL(positionChanged(const GenTime &)), this,
        SIGNAL(seekPositionChanged(const GenTime &)));*/
        /*connect(m_app, SIGNAL(positionChanged(const GenTime &)), this,
        SIGNAL(seekPositionChanged(const GenTime &)));*/

	 connect(m_render, SIGNAL(playing(double)), this,
	    SIGNAL(playSpeedChanged(double)));
	 connect(m_render, SIGNAL(stopped()), this,
	    SLOT(slotRendererStopped()));
	m_render->createVideoXWindow(false, winId());
    } 
    
    KMMScreen::~KMMScreen() {
	// TODO - the renderer needs to be unregistered from the render manager. We
	// should not delete it ourselves here.
	// if(m_render) delete m_render;
    }

    void KMMScreen::paintEvent ( QPaintEvent * ) {
        m_render->askForRefresh();
    }
    
    void KMMScreen::exportCurrentFrame(KURL url, bool notify) {
        m_render->exportCurrentFrame(url, notify);
    }

    QPixmap KMMScreen::extractFrame(int frame, int width, int height) {
        return m_render->extractFrame(frame, width, height);
    }


/** Seeks to the specified time */
    void KMMScreen::seek(const GenTime & time) {
	m_render->seek(time);
    }

/** Stop playing, reset speed to 0 and seek to clip inpoint */
    void KMMScreen::playStopped(const GenTime & startTime) {
	m_render->stop(startTime);
    }

    void KMMScreen::startRenderer() {
	m_render->start();
    }

    void KMMScreen::stopRenderer() {
	m_render->stop();
    }


    void KMMScreen::resetRenderer() {
	m_render->clear();
    }


/** Set the play speed of the screen */
    void KMMScreen::play(double speed) {
	m_render->play(speed);
    }

    void KMMScreen::play(double speed, const GenTime & startTime) {
	m_render->play(speed, startTime);
    }

    void KMMScreen::play(double speed, const GenTime & startTime,
	const GenTime & endTime) {
	m_render->play(speed, startTime, endTime);
    }


    /** Render project to firewire */
    void KMMScreen::exportToFirewire(QString url, int port, GenTime startTime, GenTime endTime)
    {
        m_render->exportFileToFirewire(url, port, startTime, endTime);
    }
    

    void KMMScreen::slotPlayingStopped()
    {
        emit playingStopped();
    }
    
    /** return current scenelist */
    QDomDocument KMMScreen::sceneList()
    {
	return m_render->sceneList();
    }

    
/** Set the displayed scenelist to the one specified. */
    void KMMScreen::setSceneList(const QDomDocument & scenelist,
	bool resetPosition) {
	m_render->setSceneList(scenelist, resetPosition);
    }

    int KMMScreen::getLength()
    {
        return m_render->getLength();
    }
    
    void KMMScreen::setTitlePreview(QString tmpFileName)
    {
        m_render->setTitlePreview(tmpFileName);
    }

    void KMMScreen::restoreProducer()
    {
        m_render->restoreProducer();
    }
    
    void KMMScreen::slotRendererStopped() {
	emit playSpeedChanged(0.0);
    }

    double KMMScreen::playSpeed() {
	return m_render->playSpeed();
    }

    const GenTime & KMMScreen::seekPosition() const {
	return m_render->seekPosition();
    }
    
    void KMMScreen::positionChanged(GenTime t) {
        emit seekPositionChanged(t);
    }

    // virtual 
    void KMMScreen::mousePressEvent(QMouseEvent * e) {
	emit mouseClicked();
	if (e->button() == RightButton) {
	    emit mouseRightClicked();
	}
    }

    void KMMScreen::mouseMoveEvent(QMouseEvent * e) {
	if ((e->state() & LeftButton) || (e->state() & RightButton)
	    || (e->state() & MidButton)) {
            // TODO: Allow dragging from workspace monitor to create a new clip from a timeline section
            if (m_name != "Document") emit mouseDragged();
	}
    }

//virtual
    void KMMScreen::wheelEvent(QWheelEvent * e) {
	GenTime newSeek =
	    seekPosition() - GenTime(e->delta() / 120,
	    m_app->getDocument()->framesPerSecond());
	if (newSeek < GenTime())
	    newSeek = GenTime(0);
	if (newSeek > m_clipLength)
	    newSeek = m_clipLength;
	seek(newSeek);
    }

    void KMMScreen::setClipLength(int frames) {
	m_clipLength =
	    GenTime(frames, m_app->getDocument()->framesPerSecond());
    }

}				// namespace Gui
