/***************************************************************************
                         kmmtrackpanel.cpp  -  description
                            -------------------
   begin                : Tue Aug 6 2002
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

#include "kmmtrackpanel.h"
#include "ktimeline.h"
#include "kdenlivedoc.h"
#include "trackpanelfunction.h"

#include <kdebug.h>
#include <iostream>

KMMTrackPanel::KMMTrackPanel( KTimeLine *timeline,
                              KdenliveDoc *document,
                              DocTrackBase *docTrack,
                              QWidget *parent,
                              const char *name ) :
		KTrackClipPanel(timeline, parent, name),
		m_docTrack( docTrack ),
		m_document( document )
{
	setMinimumWidth( 200 );
	setMaximumWidth( 200 );
	setSizePolicy( QSizePolicy( QSizePolicy::Maximum, QSizePolicy::Expanding ) );
	setPalette( QPalette( QColor( 170, 170, 170 ) ) );

	setFrameStyle( QFrame::Panel | QFrame::Sunken );
}

KMMTrackPanel::~KMMTrackPanel()
{}

/** returns the document track which is displayed by this track */
DocTrackBase * KMMTrackPanel::docTrack()
{
	return m_docTrack;
}

//virtual
void KMMTrackPanel::drawToBackBuffer( QPainter &painter, QRect &rect )
{
	KTrackClipPanel::drawToBackBuffer(painter, rect);

	// draw the vertical time marker
	int value = ( int ) timeline() ->mapValueToLocal( timeline() ->seekPosition().frames( m_document->framesPerSecond() ) );
	if ( value >= rect.x() && value <= rect.x() + rect.width() ) {
		painter.drawLine( value, rect.y(), value, rect.y() + rect.height() );
	}
}

// virtual
int KMMTrackPanel::documentTrackIndex()  const
{
	return m_document->trackIndex(m_docTrack);
}
