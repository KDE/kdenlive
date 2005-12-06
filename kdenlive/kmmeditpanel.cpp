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

#include <kled.h>
#include <qlcdnumber.h>
#include <kdebug.h>

#include "kmmeditpanel.h"

#include "kiconloader.h"
#include "qtoolbutton.h"
#include "qtooltip.h"

#include "kfixedruler.h"
#include "krulertimemodel.h"
#include "kdenlivedoc.h"

namespace Gui
{

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

	timeCode->setSegmentStyle(QLCDNumber::Flat);
	timeCode->setPaletteBackgroundColor (Qt::black);
	timeCode->setPaletteForegroundColor (Qt::green);

	tcode.setFormat(Timecode::HH_MM_SS_FF);

	KIconLoader loader;

	startButton->setPixmap( loader.loadIcon( "player_start", KIcon::Small ) );
	rewindButton->setPixmap( loader.loadIcon( "player_rew", KIcon::Small ) );
	stopButton->setPixmap( loader.loadIcon( "player_stop", KIcon::Small ) );
	playButton->setPixmap( loader.loadIcon( "player_play", KIcon::Small ) );
	forwardButton->setPixmap( loader.loadIcon( "player_fwd", KIcon::Small ) );
	endButton->setPixmap( loader.loadIcon( "player_end", KIcon::Small ) );
	inpointButton->setPixmap( loader.loadIcon( "start", KIcon::Small ) );
	outpointButton->setPixmap( loader.loadIcon( "finish", KIcon::Small ) );

	previousMarkerButton->setPixmap( loader. loadIcon("1leftarrow", KIcon::Small ) );
	nextMarkerButton->setPixmap( loader.loadIcon("1rightarrow", KIcon::Small ) );

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
		timeCode->display(tcode.getTimecode(GenTime( value, m_document->framesPerSecond() ), m_document->framesPerSecond()));
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
	QToolTip::add( renderStatus, i18n( "Renderer connected" ) );
}

void KMMEditPanel::rendererDisconnected()
{
	renderStatus->off();
	QToolTip::add( renderStatus, i18n( "Renderer not connected" ) );
}

void KMMEditPanel::setInpoint()
{
	int value = m_ruler->getSliderValue( 0 );
	m_ruler->setSliderValue( 1, value);

	//if(value >= m_ruler->getSliderValue( 2 )) {
	//	m_ruler->setSliderValue( 2, value + 8 );
	//}
}

void KMMEditPanel::setOutpoint()
{
	int value = m_ruler->getSliderValue( 0 );
	m_ruler->setSliderValue( 2, value );

	//if(m_ruler->getSliderValue( 1 ) >= value) {
	//	m_ruler->setSliderValue( 1, value - 8 );
	//}
}

void KMMEditPanel::setInpoint( const GenTime &inpoint )
{
	m_ruler->setSliderValue( 1, ( int ) inpoint.frames( m_document->framesPerSecond() ) );
}

void KMMEditPanel::setOutpoint( const GenTime &outpoint )
{
	m_ruler->setSliderValue( 2, ( int ) outpoint.frames( m_document->framesPerSecond() ) );
}

// point of the slider
GenTime KMMEditPanel::point() const
{
	return GenTime( m_ruler->getSliderValue( 0 ), m_document->framesPerSecond() );
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
		m_pauseMode = true;
	} else {
		setPlaying(true);
		m_pauseMode = false;
	}
}

void KMMEditPanel::stop()
{
	m_playSelected = true;
	m_pauseMode = false;
	m_playSpeed = 0.0;
	emit playStopped(inpoint());
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
		if ( m_pauseMode == true ) {
			emit playSpeedChanged( m_playSpeed, point());
		}
		else
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

		playButton->setPixmap( loader.loadIcon( "player_pause", KIcon::Small ) );

	} else {
		if(playButton->isOn()) {
			playButton->toggle();
		}
		if(playSectionButton->isOn()) {
			playSectionButton->toggle();
		}

		playButton->setPixmap( loader.loadIcon( "player_play", KIcon::Small ) );
	}
}

void KMMEditPanel::setSnapMarker(bool markerHere)
{
	setMarkerButton->setDown(markerHere);
}

} // namespace Gui
