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
		KMMEditPanel_UI( parent, name, fl )
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

	connect( m_ruler, SIGNAL( sliderValueChanged( int, int ) ), this, SLOT( rulerValueChanged( int, int ) ) );

	connect( startButton, SIGNAL( pressed() ), this, SLOT( seekBeginning() ) );
	connect( endButton, SIGNAL( pressed() ), this, SLOT( seekEnd() ) );

	connect( playButton, SIGNAL( pressed() ), this, SLOT( play() ) );
	connect( rewindButton, SIGNAL( pressed() ), this, SLOT( stepBack() ) );
	connect( stopButton, SIGNAL( pressed() ), this, SLOT( stop() ) );
	connect( forwardButton, SIGNAL( pressed() ), this, SLOT( stepForwards() ) );
	connect( inpointButton, SIGNAL( pressed() ), this, SLOT( setInpoint() ) );
	connect( outpointButton, SIGNAL( pressed() ), this, SLOT( setOutpoint() ) );
	connect( setMarkerButton, SIGNAL( toggled(bool) ), this, SLOT( toggleMarker() ) );
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

void KMMEditPanel::play()
{
	// play is called *before* the button actuall changes state - therefore we must see if the button
	// if Off here instead of On. (If it is off, it will be on once play finishes)
	if(!playButton->isOn()) {
		setPlaying(true, true);
	} else {
		setPlaying(false, true);
	}
}

void KMMEditPanel::stop()
{
	setPlaying(false);
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
	if(playButton->isOn()) {
		setPlaying(false);
	} else {
		setPlaying(true);
	}
}

void KMMEditPanel::setPlaying(bool play, bool reverseToggle)
{
	KIconLoader loader;
	if(play) {
		emit playSpeedChanged( 1.0 );
		if(!playButton->isOn() && !reverseToggle) {
			playButton->toggle();
		}
		playButton->setPixmap( loader.loadIcon( "player_pause", KIcon::Toolbar ) );
	} else {
		emit playSpeedChanged( 0.0 );
		if(playButton->isOn() && !reverseToggle) {
			playButton->toggle();
		}
		playButton->setPixmap( loader.loadIcon( "player_play", KIcon::Toolbar ) );
	}
}

void KMMEditPanel::toggleMarker()
{
}

void KMMEditPanel::screenPlaySpeedChanged(double speed)
{
	if(speed == 0.0) {
		if(playButton->isOn()) {
			KIconLoader loader;
			playButton->toggle();
			playButton->setPixmap( loader.loadIcon( "player_play", KIcon::Toolbar ) );
		}
	}
}
