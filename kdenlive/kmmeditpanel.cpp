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

#include <kled.h>

#include "kmmeditpanel.h"

#include "kiconloader.h"
#include "qtoolbutton.h"

#include "kfixedruler.h"
#include "krulertimemodel.h"
#include "kdenlivedoc.h"

KMMEditPanel::KMMEditPanel( KdenliveDoc *document, QWidget* parent, const char* name, WFlags fl ) :
		KMMEditPanel_UI( parent, name, fl ),
		m_playSpeed(0.0),
		m_playSelected(false)
{
	m_document = document;

	m_ruler->setRulerModel( new KRulerTimeModel() );
	m_ruler->setRange( 0, 0 );
	m_ruler->setMargin( 40 );
	m_ruler->setAutoClickSlider( 0 );

	m_ruler->addSlider( KRuler::TopMark, 0 );
	m_ruler->addSlider( KRuler::StartMark, 0 );
	m_ruler->addSlider( KRuler::EndMark, m_ruler->maxValue() );

	renderStatus->off();
	renderStatus->setColor( QColor( 0, 200, 0 ) );
	renderStatus->setFixedSize( 20, 20 );

	KIconLoader loader;

	startButton->setPixmap( loader.loadIcon( "player_start", KIcon::Toolbar ) );
	rewindButton->setPixmap( loader.loadIcon( "player_rew", KIcon::Toolbar ) );
	stopButton->setPixmap( loader.loadIcon( "player_stop", KIcon::Toolbar ) );
	playButton->setPixmap( loader.loadIcon( "player_play", KIcon::Toolbar ) );
	forwardButton->setPixmap( loader.loadIcon( "player_fwd", KIcon::Toolbar ) );
	endButton->setPixmap( loader.loadIcon( "player_end", KIcon::Toolbar ) );
	inpointButton->setPixmap( loader.loadIcon( "start", KIcon::Toolbar ) );
	outpointButton->setPixmap( loader.loadIcon( "finish", KIcon::Toolbar ) );

	previousMarkerButton->setPixmap( loader. loadIcon("1leftarrow", KIcon::Toolbar ) );
	nextMarkerButton->setPixmap( loader.loadIcon("1rightarrow", KIcon::Toolbar ) );

	connect( m_ruler, SIGNAL( sliderValueChanged( int, int ) ), this, SLOT( rulerValueChanged( int, int ) ) );

	connect( startButton, SIGNAL( pressed() ), this, SLOT( seekBeginning() ) );
	connect( endButton, SIGNAL( pressed() ), this, SLOT( seekEnd() ) );

	connect( rewindButton, SIGNAL( pressed() ), this, SLOT( stepBack() ) );
	connect( forwardButton, SIGNAL( pressed() ), this, SLOT( stepForwards() ) );
	connect( inpointButton, SIGNAL( pressed() ), this, SLOT( setInpoint() ) );
	connect( outpointButton, SIGNAL( pressed() ), this, SLOT( setOutpoint() ) );

	connect( playButton, SIGNAL( pressed()), this, SLOT( play() ) );
	connect( playSectionButton, SIGNAL( pressed() ), this, SLOT( playSelected() ) );
	connect( playButton, SIGNAL( released()), this, SLOT( updateButtons() ));
	connect( playSectionButton, SIGNAL( released()), this, SLOT( updateButtons() ));

	connect( setMarkerButton, SIGNAL( clicked() ), this, SIGNAL( toggleSnapMarker() ) );
	connect( nextMarkerButton, SIGNAL( clicked() ), this, SIGNAL( nextSnapMarkerClicked() ) );
	connect( previousMarkerButton, SIGNAL( clicked() ), this, SIGNAL( previousSnapMarkerClicked() ) );

	void nextMarkerClicked();
  void previousMarkerClicked();

	connect( stopButton, SIGNAL( pressed() ), this, SLOT( stop() ) );
	connect( stopButton, SIGNAL( pressed() ), this, SLOT( updateButtons() ) );
}

KMMEditPanel::~KMMEditPanel()
{}

/** Sets the length of the clip that we are viewing. */
void KMMEditPanel::setClipLength( int frames )
{
	m_ruler->setMaxValue( frames );
}

/** A slider on the ruler has changed value */
void KMMEditPanel::rulerValueChanged( int ID, int value )
{
	switch ( ID ) {
		case 0 :
		emit seekPositionChanged( GenTime( value, m_document->framesPerSecond() ) );
		break;
		case 1 :
		emit inpointPositionChanged( GenTime( value, m_document->framesPerSecond() ) );
		break;
		case 2 :
		emit outpointPositionChanged( GenTime( value, m_document->framesPerSecond() ) );
		break;
	}
}

/** Seeks to the beginning of the ruler. */
void KMMEditPanel::seekBeginning()
{
	m_ruler->setSliderValue( 0, m_ruler->minValue() );
}

/** Seeks to the end of the ruler */
void KMMEditPanel::seekEnd()
{
	m_ruler->setSliderValue( 0, m_ruler->maxValue() );
}

void KMMEditPanel::stepBack()
{
	m_ruler->setSliderValue( 0, m_ruler->getSliderValue( 0 ) - 1 );
}

void KMMEditPanel::stepForwards()
{
	m_ruler->setSliderValue( 0, m_ruler->getSliderValue( 0 ) + 1 );
}

void KMMEditPanel::seek( const GenTime &time )
{
	m_ruler->setSliderValue( 0, ( int ) ( floor( time.frames( m_document->framesPerSecond() ) + 0.5 ) ) );
}

void KMMEditPanel::rendererConnected()
{
	renderStatus->on();
}

void KMMEditPanel::rendererDisconnected()
{
	renderStatus->off();
}

void KMMEditPanel::setInpoint()
{
	m_ruler->setSliderValue( 1, m_ruler->getSliderValue( 0 ) );
}

void KMMEditPanel::setOutpoint()
{
	m_ruler->setSliderValue( 2, m_ruler->getSliderValue( 0 ) );
}

void KMMEditPanel::setInpoint( const GenTime &inpoint )
{
	m_ruler->setSliderValue( 1, ( int ) inpoint.frames( m_document->framesPerSecond() ) );
}

void KMMEditPanel::setOutpoint( const GenTime &outpoint )
{
	m_ruler->setSliderValue( 2, ( int ) outpoint.frames( m_document->framesPerSecond() ) );
}

GenTime KMMEditPanel::inpoint() const
{
	return GenTime( m_ruler->getSliderValue( 1 ), m_document->framesPerSecond() );
}

GenTime KMMEditPanel::outpoint() const
{
	return GenTime( m_ruler->getSliderValue( 2 ), m_document->framesPerSecond() );
}

void KMMEditPanel::togglePlay()
{
	m_playSelected = false;
	if(isPlaying()) {
		setPlaying(false);
	} else {
		setPlaying(true);
	}
	updateButtons();
}

void KMMEditPanel::togglePlaySelected()
{
	m_playSelected = true;
	if(isPlaying()) {
		setPlaying(false);
	} else {
		setPlaying(true);
	}
	updateButtons();
}

void KMMEditPanel::screenPlaySpeedChanged(double speed)
{
	m_playSpeed = speed;
	updateButtons();
}

void KMMEditPanel::playSelected()
{
	m_playSelected = true;

	if(isPlaying()) {
		setPlaying(false);
	} else {
		setPlaying(true);
	}
}

void KMMEditPanel::play()
{
	m_playSelected = false;

	if(isPlaying()) {
		setPlaying(false);
	} else {
		setPlaying(true);
	}
}

void KMMEditPanel::stop()
{
	m_playSelected = true;
	setPlaying(false);
}

void KMMEditPanel::setPlaying(bool play)
{
	if(play) {
		m_playSpeed = 1.0;
	} else {
		m_playSpeed = 0.0;
	}

	if(m_playSelected) {
		emit playSpeedChanged( m_playSpeed, inpoint(), outpoint() );
	} else {
		emit playSpeedChanged( m_playSpeed );
	}
}

void KMMEditPanel::updateButtons()
{
	KIconLoader loader;
	if(isPlaying()) {
		if(!playButton->isOn()) {
			playButton->toggle();
			if(m_playSelected) {
				if(!playSectionButton->isOn()) {
					playSectionButton->toggle();
				}
			} else {
				if(playSectionButton->isOn()) {
					playSectionButton->toggle();
				}
			}
		}

		playButton->setPixmap( loader.loadIcon( "player_pause", KIcon::Toolbar ) );

	} else {
		if(playButton->isOn()) {
			playButton->toggle();
		}
		if(playSectionButton->isOn()) {
			playSectionButton->toggle();
		}

		playButton->setPixmap( loader.loadIcon( "player_play", KIcon::Toolbar ) );
	}
}

void KMMEditPanel::setSnapMarker(bool markerHere)
{
	setMarkerButton->setDown(markerHere);
}
