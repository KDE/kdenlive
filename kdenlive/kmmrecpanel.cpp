#include <klocale.h>
/***************************************************************************
                         KMMRecPanelimplementation.cpp  -  description
                            -------------------

   copyright            : (C) 2006 by Jean-Baptiste Mardelle
   email                : jb@ader.ch
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <cmath>

#include <kled.h>
#include <qlcdnumber.h>
#include <kdebug.h>

#include "kmmrecpanel.h"

#include "kiconloader.h"
#include "qpushbutton.h"
#include "qtooltip.h"

#include "kfixedruler.h"
#include "krulertimemodel.h"
#include "kdenlivedoc.h"

namespace Gui {

    KMMRecPanel::KMMRecPanel(KdenliveDoc * document, QWidget * parent,
	const char *name, WFlags fl):KMMRecPanel_UI(parent, name, fl),
    m_playSpeed(0.0), m_playSelected(false), m_showLcd(true) {
	m_document = document;


	renderStatus->off();
	renderStatus->setColor(QColor(0, 200, 0));
	renderStatus->setFixedSize(20, 20);

	timeCode->setSegmentStyle(QLCDNumber::Flat);
	timeCode->setPaletteBackgroundColor(Qt::black);
	timeCode->setPaletteForegroundColor(Qt::green);
	timeCode->setNumDigits(11);
	tcode.setFormat(Timecode::HH_MM_SS_FF);

	KIconLoader loader;


        // get the usual font height in pixel
        buttonSize = playButton->fontInfo().pixelSize ();

	 connect(rew1Button, SIGNAL(pressed()), this, SIGNAL(stepRewindDevice()));
	 connect(rewindButton, SIGNAL(pressed()), this, SIGNAL(rewindDevice()));

	 connect(fwd1Button, SIGNAL(pressed()), this, SIGNAL(stepForwardDevice()));
	connect(forwardButton, SIGNAL(pressed()), this, SIGNAL(forwardDevice()));
	 connect(recButton, SIGNAL(pressed()), this, SIGNAL(recDevice()));
	 connect(playButton, SIGNAL(pressed()), this, SIGNAL(playDevice()));
	connect(pauseButton, SIGNAL(pressed()), this, SIGNAL(pauseDevice()));

	 connect(playButton, SIGNAL(released()), this, SLOT(updateButtons()));
         
	 connect(stopButton, SIGNAL(pressed()), this, SIGNAL(stopDevice()));
	 connect(stopButton, SIGNAL(pressed()), this,
	    SLOT(updateButtons()));

	// timecode from camera not working for the moment
	timeCode->hide();
    } 
    
    KMMRecPanel::~KMMRecPanel() {}


    
    void KMMRecPanel::showLcd(bool show) {
        m_showLcd = show;
        if (!m_showLcd) timeCode->hide();
        else timeCode->show();
    }

/** A slider on the ruler has changed value */
    void KMMRecPanel::rulerValueChanged(int ID, int value) {
	switch (ID) {
	case 0:
	    emit seekPositionChanged(GenTime(value,
		    m_document->framesPerSecond()));
            if (m_showLcd) timeCode->display(tcode.getTimecode(GenTime(value,
			m_document->framesPerSecond()), m_document->framesPerSecond()));
	    break;
	case 1:
	    emit inpointPositionChanged(GenTime(value,
		    m_document->framesPerSecond()));
	    break;
	case 2:
	    emit outpointPositionChanged(GenTime(value,
		    m_document->framesPerSecond()));
	    break;
	}
    }

/** Seeks to the beginning of the ruler. */
    void KMMRecPanel::seekBeginning() {
	
    }

/** Seeks to the end of the ruler */
    void KMMRecPanel::seekEnd() {
	
    }

    void KMMRecPanel::stepBack() {
	
    }

    void KMMRecPanel::stepForwards() {
	
    }

    void KMMRecPanel::seek(const GenTime & time) {
	
    }

    void KMMRecPanel::rendererConnected() {
	renderStatus->on();
	QToolTip::add(renderStatus, i18n("Renderer connected"));
    }

    void KMMRecPanel::rendererDisconnected() {
	renderStatus->off();
	QToolTip::add(renderStatus, i18n("Renderer not connected"));
    }

    void KMMRecPanel::setInpoint() {

    }

    void KMMRecPanel::setOutpoint() {
    }

    void KMMRecPanel::setInpoint(const GenTime & inpoint) {
    }

    void KMMRecPanel::setOutpoint(const GenTime & outpoint) {
    }

// point of the slider
    GenTime KMMRecPanel::point() const {
    } 
    
    GenTime KMMRecPanel::inpoint() const {
    } 
    
    GenTime KMMRecPanel::outpoint() const {
    } 
    
    void KMMRecPanel::togglePlaySelected() {
	m_loop = false;
	m_playSelected = true;
	if (isPlaying()) {
	    setPlaying(false);
	} else {
	    setPlaying(true);
	}
    }

    void KMMRecPanel::toggleForward() {
	m_playSelected = false;
	if (m_playSpeed < 0.0) m_playSpeed = 2;
        else m_playSpeed = m_playSpeed * 2;
	if (m_playSpeed == 0) m_playSpeed = 2;
 	if (m_playSpeed == 32) m_playSpeed = 2;
	setPlaying(true);
    }

    void KMMRecPanel::toggleRewind() {
	m_playSelected = false;
	if (m_playSpeed > 0.0) m_playSpeed = -2; 
        else m_playSpeed = m_playSpeed * 2;
	if (m_playSpeed == 0) m_playSpeed = -2;
 	if (m_playSpeed == -32) m_playSpeed = -2;
	setPlaying(true);
    }

    void KMMRecPanel::screenPlaySpeedChanged(double speed) {
	if (m_pauseMode) return;
	m_playSpeed = speed;
	updateButtons();
    }
    
    void KMMRecPanel::screenPlayStopped() {
	if (m_loop) {
	    m_playSelected = true;
	    setPlaying(true);
	    return;
	}
	if (m_pauseMode) return;
        m_playSpeed = 0.0;
        updateButtons();
    }

    void KMMRecPanel::loopSelected() {
	m_playSelected = true;
	m_pauseMode = false;
	m_loop = true;

	if (isPlaying()) {
	    setPlaying(false);
	} else {
	    setPlaying(true);
	}
	//updateButtons();
    }

    void KMMRecPanel::play() {
	m_playSelected = false;
	m_loop = false;
	if (isPlaying() && (m_playSpeed == 1.0)) {
	    setPlaying(false);
	    return;
	}
	if (m_pauseMode == false) m_startPlayPosition = point();
        m_playSpeed = 1.0;
	setPlaying(true);
    }

    void KMMRecPanel::stop() {
	m_playSelected = true;
	m_pauseMode = false;
	m_loop = false;
	m_playSpeed = 0.0;
	emit playStopped(m_startPlayPosition);
    }

    void KMMRecPanel::setPlaying(bool play) {
	double playSpeed;
	if (play && m_playSpeed == 0.0) m_playSpeed = 1.0;
	m_pauseMode = !play;
	if (play) {
		playSpeed =  m_playSpeed;
	}
	else {
		playSpeed = 0.0;
	}

	emit activateMonitor();

	if (m_playSelected) {
            emit playSpeedChanged(playSpeed, inpoint(), outpoint());
	} else {
	    if (m_pauseMode == true) {
		emit playSpeedChanged(playSpeed, point());
	    } else 
		emit playSpeedChanged(playSpeed);
        }
	updateButtons();
    }

    void KMMRecPanel::updateButtons() {
	KIconLoader loader;
	stopButton->setDown(false);
	if (isPlaying()) {
	    forwardButton->setDown(isForwarding());
	    rewindButton->setDown(isRewinding());
	    if (!playButton->isOn()) {
		playButton->toggle();
	    }

	    playButton->setPixmap(loader.loadIcon("kdenlive_pause",
                                  KIcon::Small, buttonSize));

	} else {
	    if (playButton->isOn()) playButton->toggle();
	    playButton->setPixmap(loader.loadIcon("kdenlive_play",
                                  KIcon::Small, buttonSize));
	}
    }

    void KMMRecPanel::setSnapMarker(bool markerHere) {

    }

}				// namespace Gui
