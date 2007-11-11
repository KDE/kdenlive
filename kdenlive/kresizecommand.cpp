/***************************************************************************
                          kresizecommand.cpp  -  description
                             -------------------
    begin                : Sat Dec 14 2002
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

#include "kresizecommand.h"
#include "docclipref.h"
#include "kdenlivedoc.h"

#include <klocale.h>
#include <kdebug.h>

namespace Command {

    KResizeCommand::KResizeCommand(KdenliveDoc * doc, DocClipRef & clip) {
	m_doc = doc;
	m_trackNum = clip.trackNum();
	m_end_trackEnd = m_start_trackEnd = clip.trackEnd();
	m_end_trackStart = m_start_trackStart = clip.trackStart();
	m_end_cropStart = m_start_cropStart = clip.cropStartTime();

	TransitionStack stack = clip.clipTransitions();
	TransitionStack::iterator itt = stack.begin();
	// remember transitions start pos
	while (itt != stack.end()) {
	    m_transitionStartPoints.append(QPoint((*itt)->transitionStartTime().frames(25), (*itt)->transitionEndTime().frames(25)));
	    ++itt;
	}
    } 

    KResizeCommand::~KResizeCommand() {
    }

    void KResizeCommand::setEndSize(DocClipRef & clip) {
	m_end_trackEnd = clip.trackEnd();
	m_end_trackStart = clip.trackStart();
	m_end_cropStart = clip.cropStartTime();

	TransitionStack stack = clip.clipTransitions();
	TransitionStack::iterator itt = stack.begin();
	// remember transitions start pos
	while (itt != stack.end()) {
	    m_transitionEndPoints.append(QPoint((*itt)->transitionStartTime().frames(25), (*itt)->transitionEndTime().frames(25)));
	    ++itt;
	}
    }

/** Returns the name of this command */
    QString KResizeCommand::name() const {
	return i18n("Resize clip");
    }

/** Executes this command */ 
    void KResizeCommand::execute() {
	DocClipRef *clip =
	    m_doc->track(m_trackNum)->getClipAt((m_start_trackStart +
		m_start_trackEnd) / 2.0);
	if (!clip) clip = m_doc->track(m_trackNum)->getClipAt((m_end_trackStart +
		m_end_trackEnd) / 2.0);
	if (!clip) {
	    kdWarning() <<
		"ResizeCommand execute failed - cannot find clip!!!" << m_start_trackStart.frames(25)<<", "<<m_start_trackEnd.frames(25)<<endl;
	} else {
	    if (m_end_trackStart == m_start_trackStart) {
		kdDebug()<<" / / / resizing end: "<<m_end_trackStart.frames(25)<<", "<<clip->trackStart().frames(25)<<endl;
		// resizing clip end
	        // clip->setTrackStart(m_end_trackStart);
	        // clip->setCropStartTime(m_end_cropStart);
		clip->setTrackEnd(m_end_trackEnd);
		if (clip->hasVariableThumbnails()) clip->doFetchEndThumbnail();
		m_doc->renderer()->mltResizeClipEnd(clip->playlistTrackNum(), m_start_trackStart, m_end_cropStart, m_end_cropStart + m_end_trackEnd - m_end_trackStart);
		m_doc->redrawTimelineSection(clip->trackNum(), m_start_trackEnd, m_end_trackEnd);
		m_doc->slotCheckCurrentTransition();
	    }
	    else {
		// resizing clip start
	        clip->setTrackStart(m_end_trackStart);
	        clip->setCropStartTime(m_end_cropStart);
		clip->setTrackEnd(m_end_trackEnd);
		if (clip->hasVariableThumbnails()) clip->doFetchStartThumbnail();
		m_doc->renderer()->mltResizeClipStart(clip->playlistTrackNum(), m_start_trackEnd, m_end_trackStart, m_start_trackStart, m_end_cropStart, m_end_cropStart + m_end_trackEnd - m_end_trackStart);
		m_doc->redrawTimelineSection(clip->trackNum(), m_end_cropStart, m_end_cropStart + m_end_trackEnd - m_end_trackStart);
		m_doc->slotCheckCurrentTransition();
	    }

	    // move clip transitions
	    TransitionStack stack = clip->clipTransitions();
    	    if (!stack.isEmpty()) {
        	TransitionStack::iterator itt = stack.begin();
		QValueList < QPoint >::Iterator it = m_transitionStartPoints.begin();
		QValueList < QPoint >::Iterator endIt = m_transitionEndPoints.begin();
        	while (itt != stack.end() && it != m_transitionStartPoints.end()) {
		    m_doc->renderer()->mltMoveTransition((*itt)->transitionTag(), (*itt)->transitionStartTrack(), 0, GenTime((*it).x(), 25), GenTime((*it).y(), 25),  GenTime((*endIt).x(), 25), GenTime((*endIt).y(), 25));
            	    ++it;
		    ++endIt;
            	    ++itt;
        	}
    	    }
	}
	//m_doc->indirectlyModified();
    }

/** Unexecutes this command */
    void KResizeCommand::unexecute() {
	DocClipRef *clip =
	    m_doc->track(m_trackNum)->getClipAt(m_end_trackStart +
	    ((m_end_trackEnd - m_end_trackStart) / 2.0));
	if (!clip) clip = m_doc->track(m_trackNum)->getClipAt((m_end_trackStart +
		m_end_trackEnd) / 2.0);
	if (!clip) {
	    kdWarning() <<
		"ResizeCommand unexecute failed - cannot find clip!!!" <<
		endl;
	} else {
	    if (m_start_trackStart == m_end_trackStart) {
		// resizing clip end
	    	// clip->setTrackStart(m_start_trackStart);
	    	// clip->setCropStartTime(m_start_cropStart);
	    	clip->setTrackEnd(m_start_trackEnd);
	    	if (clip->hasVariableThumbnails()) clip->doFetchEndThumbnail();
		m_doc->renderer()->mltResizeClipEnd(clip->playlistTrackNum(), clip->trackMiddleTime(), m_start_cropStart, m_start_cropStart + m_start_trackEnd - m_start_trackStart);
		m_doc->redrawTimelineSection(clip->trackNum(), m_start_trackEnd, m_end_trackEnd);
		m_doc->slotCheckCurrentTransition();
	    }
	    else {
		// resizing clip start
	    	clip->setTrackStart(m_start_trackStart);
	    	clip->setCropStartTime(m_start_cropStart);
	    	clip->setTrackEnd(m_start_trackEnd);
	    	if (clip->hasVariableThumbnails()) clip->doFetchStartThumbnail();
		m_doc->redrawTimelineSection(clip->trackNum(), m_start_trackStart, m_end_trackStart);
		m_doc->renderer()->mltResizeClipStart(clip->playlistTrackNum(), clip->trackMiddleTime(), m_start_trackStart, m_end_trackStart, m_start_cropStart, m_start_cropStart + m_start_trackEnd - m_start_trackStart);
		m_doc->slotCheckCurrentTransition();
	    }

	    // move clip transitions
	    TransitionStack stack = clip->clipTransitions();
    	    if (!stack.isEmpty()) {
        	TransitionStack::iterator itt = stack.begin();
		QValueList < QPoint >::Iterator it = m_transitionStartPoints.begin();
		QValueList < QPoint >::Iterator endIt = m_transitionEndPoints.begin();
        	while (itt != stack.end() && it != m_transitionStartPoints.end()) {
		    m_doc->renderer()->mltMoveTransition((*itt)->transitionTag(), (*itt)->transitionStartTrack(), 0, GenTime((*endIt).x(), 25), GenTime((*endIt).y(), 25),  GenTime((*it).x(), 25), GenTime((*it).y(), 25));
            	    ++it;
		    ++endIt;
            	    ++itt;
        	}
    	    }

	}
	//m_doc->indirectlyModified();
    }

/** Sets the trackEnd() for the end destination to the time specified. */
    void KResizeCommand::setEndTrackEnd(const GenTime & time) {
	m_end_trackEnd = time;
    }

    void KResizeCommand::setEndTrackStart(const GenTime & time) {
	m_end_trackStart = time;
    }

}				// namespace Command
