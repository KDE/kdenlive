#include <klocale.h>
/***************************************************************************
                         kmmeditpanelimplementation.cpp  -  description
                            -------------------
   begin                : Mon Apr 8 2002
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

#include <cmath>

#include "qpushbutton.h"
#include "qtooltip.h"
#include "qslider.h"
#include "qcursor.h"

#include <kled.h>
#include <kdebug.h>
#include <kdatetbl.h>
#include <kiconloader.h>
#include <krestrictedline.h>

#include "kmmeditpanel.h"


#include "kfixedruler.h"
#include "krulertimemodel.h"
#include "kdenlivedoc.h"

namespace Gui {

    KMMEditPanel::KMMEditPanel(KdenliveDoc * document, QWidget * parent,
	const char *name, WFlags fl):KMMEditPanel_UI(parent, name, fl),
    m_playSpeed(0.0), m_playSelected(false), m_loop(false), m_startPlayPosition(0), m_volume(1.0) {
	m_document = document;

	m_ruler->setRulerModel(new KRulerTimeModel());
	m_ruler->setRange(0, 0);
	m_ruler->setMargin(40);
	m_ruler->setAutoClickSlider(0);

	m_ruler->addSlider(KRuler::TopMark, 0);
	m_ruler->addSlider(KRuler::StartMark, 0);
	m_ruler->addSlider(KRuler::EndMark, m_ruler->maxValue());

	renderStatus->off();
	renderStatus->setColor(QColor(0, 200, 0));
	renderStatus->setFixedSize(20, 20);

	tcode.setFormat(Timecode::HH_MM_SS_FF);

	KIconLoader loader;


        // get the usual font height in pixel
        buttonSize = startButton->fontInfo().pixelSize ();
        
	 /*startButton->setIconSet(QIconSet(loader.loadIcon("player_start",
                                 KIcon::Small, buttonSize)));
	 rewindButton->setIconSet(QIconSet(loader.loadIcon("player_rew",
                                  KIcon::Small, buttonSize)));
	 stopButton->setIconSet(QIconSet(loader.loadIcon("player_stop",
                                KIcon::Small, buttonSize)));
         playButton->setPixmap(loader.loadIcon("player_play",
                               KIcon::Small, buttonSize));
	 forwardButton->setIconSet(QIconSet(loader.loadIcon("player_fwd",
                                   KIcon::Small, buttonSize)));
	 endButton->setIconSet(QIconSet(loader.loadIcon("player_end",
                               KIcon::Small, buttonSize)));
	 inpointButton->setIconSet(QIconSet(loader.loadIcon("start",
                                   KIcon::Small, buttonSize)));
	 outpointButton->setIconSet(QIconSet(loader.loadIcon("finish",
                                    KIcon::Small, buttonSize)));

	 previousMarkerButton->setIconSet(QIconSet(loader.
		loadIcon("1leftarrow", KIcon::Small)));
	 nextMarkerButton->setIconSet(QIconSet(loader.
		loadIcon("1rightarrow", KIcon::Small)));
         
         startButton->setFlat(true);
         rewindButton->setFlat(true);
         stopButton->setFlat(true);
         playButton->setFlat(true);
         forwardButton->setFlat(true);
         endButton->setFlat(true);
         inpointButton->setFlat(true);
         outpointButton->setFlat(true);
         previousMarkerButton->setFlat(true);
         nextMarkerButton->setFlat(true);
         setMarkerButton->setFlat(true);
         playSectionButton->setFlat(true);*/

	 connect(m_ruler, SIGNAL(sliderValueChanged(int, int)), this,
	    SLOT(rulerValueChanged(int, int)));

	 connect(startButton, SIGNAL(pressed()), this,
	    SLOT(seekBeginning()));
	 connect(endButton, SIGNAL(pressed()), this, SLOT(seekEnd()));

	 connect(rew1Button, SIGNAL(pressed()), this, SLOT(stepBack()));
	 connect(rewindButton, SIGNAL(pressed()), this, SLOT(toggleRewind()));

	 connect(fwd1Button, SIGNAL(pressed()), this,
	    SLOT(stepForwards()));
	connect(forwardButton, SIGNAL(pressed()), this,
	    SLOT(toggleForward()));
	 connect(inpointButton, SIGNAL(pressed()), this,
	    SLOT(setInpoint()));
	 connect(outpointButton, SIGNAL(pressed()), this,
	    SLOT(setOutpoint()));

	 connect(playButton, SIGNAL(pressed()), this, SLOT(play()));
	 connect(playButton, SIGNAL(released()), this, SLOT(updateButtons()));
         
         connect(playSectionButton, SIGNAL(pressed()), this, SLOT(togglePlaySelected()));
	 connect(playSectionButton, SIGNAL(released()), this, SLOT(updateButtons()));

         connect(loopSection, SIGNAL(pressed()), this, SLOT(loopSelected()));
	 connect(loopSection, SIGNAL(released()), this, SLOT(updateButtons()));

	 connect(setMarkerButton, SIGNAL(clicked()), this,
	    SIGNAL(toggleSnapMarker()));
	 connect(nextMarkerButton, SIGNAL(clicked()), this,
	    SIGNAL(nextSnapMarkerClicked()));
	 connect(previousMarkerButton, SIGNAL(clicked()), this,
	    SIGNAL(previousSnapMarkerClicked()));

	 connect(edit_timecode, SIGNAL(returnPressed(const QString &)), this,
	    SLOT(slotSeekToPos(const QString &)));

	 connect(volumeButton, SIGNAL(pressed()), this, SLOT(slotShowVolumeControl()));

	 connect(stopButton, SIGNAL(pressed()), this, SLOT(stop()));
	 connect(stopButton, SIGNAL(pressed()), this,
	    SLOT(updateButtons()));
    } 
    
    KMMEditPanel::~KMMEditPanel() {}

    void KMMEditPanel::slotSeekToPos(const QString &pos) {
	GenTime duration = m_document->getTimecodePosition(pos);
	seek( duration );
	edit_timecode->clearFocus();
    }

    void KMMEditPanel::slotShowVolumeControl() {
	   KPopupFrame *volumeControl = new KPopupFrame(this);
	   volumeControl->setFrameStyle(QFrame::Box | QFrame::Plain);
	   volumeControl->setMargin(3);
	   volumeControl->setLineWidth(1);
	   QSlider *volumeSlider = new QSlider(volumeControl);
	   volumeSlider->setOrientation(Qt::Horizontal);
	   volumeSlider->setValue(m_volume * 100);
	   volumeControl->setMainWidget(volumeSlider);
	   connect(volumeSlider, SIGNAL(valueChanged(int)), this, SLOT(slotEmitVolume(int)));
	   volumeControl->exec(QCursor::pos());
	   delete volumeSlider;
	   delete volumeControl;
    }

    void KMMEditPanel::slotEmitVolume(int volume) {
	   m_volume = ((double) volume) / 100.0;
	   emit setVolume( m_volume );
    }

/** Sets the length of the clip that we are viewing. */
    void KMMEditPanel::setClipLength(int frames) {
	m_ruler->setMaxValue(frames);
    }
    
/** A slider on the ruler has changed value */
    void KMMEditPanel::rulerValueChanged(int ID, int value) {
	switch (ID) {
	case 0:
	    emit seekPositionChanged(GenTime(value,
		    m_document->framesPerSecond()));
            edit_timecode->setText(tcode.getTimecode(GenTime(value,
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
    void KMMEditPanel::seekBeginning() {
	m_ruler->setSliderValue(0, m_ruler->minValue());
    }

/** Seeks to the end of the ruler */
    void KMMEditPanel::seekEnd() {
	m_ruler->setSliderValue(0, m_ruler->maxValue());
    }

    void KMMEditPanel::stepBack() {
	m_ruler->setSliderValue(0, m_ruler->getSliderValue(0) - 1);
    }

    void KMMEditPanel::stepForwards() {
	m_ruler->setSliderValue(0, m_ruler->getSliderValue(0) + 1);
    }

    void KMMEditPanel::seek(const GenTime & time) {
	m_ruler->setSliderValue(0, (int) (time.frames(m_document->framesPerSecond())));
    }

    void KMMEditPanel::rendererConnected() {
	renderStatus->on();
	QToolTip::add(renderStatus, i18n("Renderer connected"));
    }

    void KMMEditPanel::rendererDisconnected() {
	renderStatus->off();
	QToolTip::add(renderStatus, i18n("Renderer not connected"));
    }

    void KMMEditPanel::setInpoint() {
	int value = m_ruler->getSliderValue(0);
	m_ruler->setSliderValue(1, value);

	//if(value >= m_ruler->getSliderValue( 2 )) {
	//      m_ruler->setSliderValue( 2, value + 8 );
	//}
    }

    void KMMEditPanel::setOutpoint() {
	int value = m_ruler->getSliderValue(0);
	m_ruler->setSliderValue(2, value + 1);

	//if(m_ruler->getSliderValue( 1 ) >= value) {
	//      m_ruler->setSliderValue( 1, value - 8 );
	//}
    }

    void KMMEditPanel::setInpoint(const GenTime & inpoint) {
	m_ruler->setSliderValue(1,
	    (int) inpoint.frames(m_document->framesPerSecond()));
    }

    void KMMEditPanel::setOutpoint(const GenTime & outpoint) {
	m_ruler->setSliderValue(2,
	    (int) outpoint.frames(m_document->framesPerSecond()));
    }

// point of the slider
    GenTime KMMEditPanel::point() const {
	return GenTime(m_ruler->getSliderValue(0),
	    m_document->framesPerSecond());
    } 
    
    GenTime KMMEditPanel::inpoint() const {
	return GenTime(m_ruler->getSliderValue(1),
	    m_document->framesPerSecond());
    } 
    
    GenTime KMMEditPanel::outpoint() const {
	return GenTime(m_ruler->getSliderValue(2) - 1,
	    m_document->framesPerSecond());
    } 
    
    void KMMEditPanel::togglePlaySelected() {
	m_loop = false;
	m_playSelected = true;
	if (isPlaying()) {
	    setPlaying(false);
	} else {
	    setPlaying(true);
	}
    }

    void KMMEditPanel::toggleForward() {
	m_playSelected = false;
	if (m_playSpeed < 0.0) m_playSpeed = 2;
        else m_playSpeed = m_playSpeed * 2;
	if (m_playSpeed == 0) m_playSpeed = 2;
 	if (m_playSpeed == 32) m_playSpeed = 2;
	setPlaying(true);
    }

    void KMMEditPanel::toggleRewind() {
	m_playSelected = false;
	if (m_playSpeed > 0.0) m_playSpeed = -2; 
        else m_playSpeed = m_playSpeed * 2;
	if (m_playSpeed == 0) m_playSpeed = -2;
 	if (m_playSpeed == -32) m_playSpeed = -2;
	setPlaying(true);
    }

    void KMMEditPanel::screenPlaySpeedChanged(double speed) {
	if (m_pauseMode) return;
	m_playSpeed = speed;
	updateButtons();
    }
    
    void KMMEditPanel::screenPlayStopped() {
	if (m_loop) {
	    m_playSelected = true;
	    setPlaying(true);
	    return;
	}
	if (m_pauseMode) return;
        m_playSpeed = 0.0;
        updateButtons();
    }

    void KMMEditPanel::loopSelected() {
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

    void KMMEditPanel::play() {
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

    void KMMEditPanel::stop() {
	m_playSelected = true;
	m_pauseMode = false;
	m_loop = false;
	m_playSpeed = 0.0;
	emit playStopped(m_startPlayPosition);
    }

    void KMMEditPanel::setPlaying(bool play) {
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

    void KMMEditPanel::updateButtons() {
	KIconLoader loader;
	stopButton->setDown(false);
	if (m_loop) { 
	    if (!loopSection->isOn()) loopSection->toggle();
	    if (!playButton->isOn()) playButton->toggle();
	    playButton->setPixmap(loader.loadIcon("kdenlive_pause",
                                  KIcon::Small, buttonSize));
	    return;
	}
	if (isPlaying()) {
	    forwardButton->setDown(isForwarding());
	    rewindButton->setDown(isRewinding());
	    if (!playButton->isOn()) {
		playButton->toggle();
		if (m_playSelected) {
		    if (!playSectionButton->isOn()) {
			playSectionButton->toggle();
		    }
		} else {
		    if (playSectionButton->isOn()) {
			playSectionButton->toggle();
		    }
		}
	    }

	    playButton->setPixmap(loader.loadIcon("kdenlive_pause",
                                  KIcon::Small, buttonSize));

	} else {
	    if (playButton->isOn()) playButton->toggle();
	    if (playSectionButton->isOn()) playSectionButton->toggle();
	    if (loopSection->isOn()) loopSection->toggle();
	    playButton->setPixmap(loader.loadIcon("kdenlive_play",
                                  KIcon::Small, buttonSize));
	}
    }

    void KMMEditPanel::setSnapMarker(bool markerHere) {
	setMarkerButton->setDown(markerHere);
    }

}				// namespace Gui
