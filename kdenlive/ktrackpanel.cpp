/***************************************************************************
                          ktrackpanel  -  description
                             -------------------
    begin                : Wed Jan 7 2004
    copyright            : (C) 2004 by Jason Wood
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
#include "ktrackpanel.h"

#include "trackviewdecorator.h"

KTrackPanel::KTrackPanel(KTimeLine *timeline,
                              		QWidget *parent,
                              		const char *name) :
			QHBox( parent, name ),
			m_timeline( timeline )
{
}


KTrackPanel::~KTrackPanel()
{
}

// virtual
void KTrackPanel::drawToBackBuffer( QPainter &painter, QRect &rect )
{
	QPtrListIterator<TrackViewDecorator> itt( m_trackViewDecorators );

	while ( itt.current() ) {
		itt.current() ->drawToBackBuffer( painter, rect );
		++itt;
	}
}


void KTrackPanel::addFunctionDecorator( const QString &mode, const QString &function )
{
	m_trackPanelFunctions[ mode ].append( function );
}

void KTrackPanel::addViewDecorator( TrackViewDecorator *view )
{
	// It is anticipated that we will add extra "modes" at some point. I have put the autodelete line here in the meantime
	// to remind me that each mode needs to be set to autodelete.
	m_trackViewDecorators.setAutoDelete( true );
	m_trackViewDecorators.append( view );
}

QStringList KTrackPanel::applicableFunctions( const QString &mode )
{
	return m_trackPanelFunctions[ mode ];
}
