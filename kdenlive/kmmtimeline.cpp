/***************************************************************************
                         kmmtimeline  -  description
                            -------------------
   begin                : Wed Dec 24 2003
   copyright            : (C) 2003 by Jason Wood
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
#include "kmmtimeline.h"
#include "kmmrulerpanel.h"
#include "kcombobox.h"

KMMTimeLine::KMMTimeLine( QWidget *scrollToolWidget, QWidget *parent , const char *name ) :
		KTimeLine( new KMMRulerPanel( NULL, "Ruler Panel" ), scrollToolWidget, parent, name )
{
	// HACK - to remove this dynamic cast, we need to change the way the relationship between KTimeline and KMMTimeline works. Either :
	// 1. We only construct KMMTimeLine via static methods, then we can create the KMMRulerPanel in the static method and pass it to
	// KMMTImeLine (and through that, KTimeLine).
	// 2. We allow the KTimeLine rulerToolWidget to be changed dynamically at runtime. Then we can set the rulerToolWidget at a later time -
	// i.e. when the constructor has had chance to construct a KMMRulerPanel so that KMMTimeLine has a typed copy of it.
	//
	// Of the two solutions, I prefer 2.
	m_rulerToolWidget = dynamic_cast<KMMRulerPanel *>( rulerToolWidget() );

	connect( m_rulerToolWidget, SIGNAL( timeScaleChanged( double ) ), this, SLOT( setTimeScale( double ) ) );
}

KMMTimeLine::~KMMTimeLine()
{}

void KMMTimeLine::invalidateClipBuffer( DocClipRef *clip )
{
	#warning - unoptimised, should only update that part of the back buffer that needs to be updated. Current implementaion
	#warning - wipes the entire buffer.
	invalidateBackBuffer();
}

void KMMTimeLine::fitToWidth()
{
	double duration = projectLength().frames( framesPerSecond() );
	if ( duration < 1.0 ) duration = 1.0;

	double scale = ( double ) viewWidth() / duration;
	m_rulerToolWidget->setScale( scale );

	setTimeScale(scale);
}

void KMMTimeLine::setSliderIndex( int index )
{
	m_rulerToolWidget->comboScaleChange( index );
}

int KMMTimeLine::getTimeScaleSliderValue() const
{
	int value = m_rulerToolWidget->m_scaleCombo->currentItem();
	return value;
}
