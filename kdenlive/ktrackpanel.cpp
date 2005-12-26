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
#include <assert.h>

#include "kplacer.h"
#include "trackviewdecorator.h"

namespace Gui
{

KTrackPanel::KTrackPanel(KTimeLine *timeline,
					KPlacer *placer,
					TRACKTYPE trackType,
                              		QWidget *parent,
                              		const char *name) :
			QHBox( parent, name ),
			m_timeline( timeline ),
			m_placer(placer),
			m_trackType(trackType),
			m_trackIsCollapsed(false)
{
	assert(timeline);
	assert(placer);
}


KTrackPanel::~KTrackPanel()
{
	if(m_placer) delete m_placer;
}

// virtual
void KTrackPanel::drawToBackBuffer( QPainter &painter, QRect &rect )
{
	QPtrListIterator<TrackViewDecorator> itt( m_trackViewDecorators );

	while ( itt.current() ) {
		m_placer->drawToBackBuffer( painter, rect, itt.current() );
		++itt;
}
}


void KTrackPanel::addFunctionDecorator( const QString &mode, const QString &function )
{
	m_trackPanelFunctions[ mode ].append( function );
}


void KTrackPanel::clearViewDecorators()
{
	m_trackViewDecorators.clear();
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

/** Returns true if this track panel has a document track index. */
//virtual
bool KTrackPanel::hasDocumentTrackIndex() const
{
	return m_placer->hasDocumentTrackIndex();
}

   /** Returns the track index into the underlying document model used by this track. Returns -1 if this is inapplicable. */
// virtual
int KTrackPanel::documentTrackIndex()  const
{
	return m_placer->documentTrackIndex();
}

/** Returns the track type (video, sound,...) */
TRACKTYPE KTrackPanel::trackType()
{
	return m_trackType;
}

/** Returns the track state (collapsed or not) */
bool KTrackPanel::isTrackCollapsed()
{
	return m_trackIsCollapsed;
}

} // namespace Gui
