/***************************************************************************
                          doctrackclipiterator.cpp  -  description
                             -------------------
    begin                : Sat Nov 30 2002
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

#include "doctrackclipiterator.h"

DocTrackClipIterator::DocTrackClipIterator(const DocTrackBase &track)
{
	m_selectedItt = new QPtrListIterator<DocClipBase>(track.firstClip(true));
	m_unselectedItt = new QPtrListIterator<DocClipBase>(track.firstClip(false));

	if(m_selectedItt->current()) {
		if(m_unselectedItt->current()) {
			m_curSelected = m_selectedItt->current()->trackStart() < m_unselectedItt->current()->trackStart();
		} else {
			m_curSelected = true;
		}
	} else {
		m_curSelected = false;
	}
}

DocTrackClipIterator::~DocTrackClipIterator()
{
	if(m_selectedItt) delete m_selectedItt;
	if(m_unselectedItt) delete m_unselectedItt;
}

/** Returns the current clip in the list, or returns 0 otherwise. */
DocClipBase * DocTrackClipIterator::current()
{
	return m_curSelected ? m_selectedItt->current() : m_unselectedItt->current();
}

/** Increments the iterator. Works identically to all other iterators. */
DocClipBase * DocTrackClipIterator::operator++()
{
	if(current() == 0) return 0;

	GenTime curTime = (m_curSelected ? m_selectedItt : m_unselectedItt)->current()->trackStart();

	while((m_selectedItt->current()) && (m_selectedItt->current()->trackStart() <= curTime)) ++(*m_selectedItt);
	while((m_unselectedItt->current()) && (m_unselectedItt->current()->trackStart() <= curTime)) ++(*m_unselectedItt);
	
	if(m_selectedItt->current()) {
		if(m_unselectedItt->current()) {
			m_curSelected = (m_selectedItt->current()->trackStart() <
																		m_unselectedItt->current()->trackStart()) ? true : false;
		} else {
			m_curSelected = true;
		}
	} else if(m_unselectedItt->current()) {
		m_curSelected = false;
	} else {
		// nothing happens, we are already at the end of the list.		
	}
		
	return current();
}
