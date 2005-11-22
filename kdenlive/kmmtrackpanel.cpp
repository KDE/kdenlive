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

namespace Gui
{

KMMTrackPanel::KMMTrackPanel( KTimeLine *timeline,
                              KdenliveDoc *document,
			      KPlacer *placer,
			      TRACKTYPE trackType,
                              QWidget *parent,
                              const char *name ) :
		KTrackPanel(timeline, placer, trackType, parent, name),
		m_document( document )
{
	setSizePolicy( QSizePolicy( QSizePolicy::Maximum, QSizePolicy::Expanding ) );
	setPalette( QPalette( QColor( 170, 170, 170 ) ) );

	setFrameStyle( QFrame::Panel | QFrame::Sunken );
}

KMMTrackPanel::~KMMTrackPanel()
{}

//virtual
void KMMTrackPanel::drawToBackBuffer( QPainter &painter, QRect &rect )
{
	KTrackPanel::drawToBackBuffer(painter, rect);

	// draw the vertical time marker
	int value = ( int ) timeline() ->mapValueToLocal( timeline() ->seekPosition().frames( m_document->framesPerSecond() ) );
	if ( value >= rect.x() && value <= rect.x() + rect.width() ) {
		painter.drawLine( value, rect.y(), value, rect.y() + rect.height() );
	}
}

} // namespace Gui
