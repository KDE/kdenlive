/***************************************************************************
                          kmoveclipscommand.cpp  -  description
                             -------------------
    begin                : Thu Dec 12 2002
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

#include "kmoveclipscommand.h"
#include "kdenlivedoc.h"
#include "docclipproject.h"
#include <klocale.h>
#include <kdebug.h>

namespace Command {

    KMoveClipsCommand::KMoveClipsCommand(KdenliveDoc * doc,
	DocClipRef * master)
    :KCommand(), m_doc(doc), m_endTime(0) {
	m_startTrack = doc->trackIndex(doc->findTrack(master));
	if (m_startTrack == -1) {
	    kdError() <<
		"KMoveClipCommand created, but master clip is not in document!"
		<< endl;
	    return;
	}
	// Choose a "safe" time in the middle of the master clip, so that we are certain to be able to find it again when// we need to.
	    m_startTime = master->trackStart();
    }

    KMoveClipsCommand::~KMoveClipsCommand() {
    }

// virtual
    QString KMoveClipsCommand::name() const {
	return i18n("Move Clips");
    }
/** Specifies where the master clip should be moved to by this command. */
    void KMoveClipsCommand::setEndLocation(DocClipRef * master) {
	m_endTrack = m_doc->trackIndex(m_doc->findTrack(master));
	m_endTime = master->trackStart();
    }

    void KMoveClipsCommand::setClipList(const DocClipRefList & list) {
	m_clipList = list;
    /*QPtrListIterator < DocClipRef > itt(list);
	while (itt.current()) {
	    m_clipList.append(QPoint((*itt)->playlistTrackNum(), (*itt)->trackStart().frames(m_doc->framesPerSecond())));
	    ++itt;
	}*/
    }


/** Executes this command */
    void KMoveClipsCommand::execute() {
	int startTrack = m_doc->projectClip().playlistTrackNum(m_startTrack);
	int endTrack = m_doc->projectClip().playlistTrackNum(m_endTrack);
	int trackOffset = endTrack - startTrack;
	int startOffset = (m_endTime - m_startTime).frames(m_doc->framesPerSecond());

	QPtrListIterator < DocClipRef > itt(m_clipList);
    	while (itt.current() != 0) {
	    int track = itt.current()->playlistTrackNum();
	    int start = itt.current()->trackStart().frames(m_doc->framesPerSecond());
  	    m_doc->renderer()->mltMoveClip(track, track + trackOffset, start, start + startOffset);

	    // Move clip transitions
	    TransitionStack stack = itt.current()->clipTransitions();
	    TransitionStack::iterator it = stack.begin();
    	    while (it) {
		m_doc->renderer()->mltMoveTransition((*it)->transitionTag(), (*it)->transitionStartTrack(), trackOffset, (*it)->transitionStartTime(), (*it)->transitionEndTime(), (*it)->transitionStartTime() + m_endTime - m_startTime, (*it)->transitionEndTime() + m_endTime - m_startTime);
        	++it;
    	    }
            ++itt;
        }

/*
        QValueList < QPoint >::Iterator it = m_clipList.begin();
        for ( it = m_clipList.begin(); it != m_clipList.end(); ++it ) {
	    //kdDebug()<<" / / /COMMAND: MOVE FM track: "<<(*it).x()<<", to: "<<(*it).x() + trackOffset<<"; START TIME: "<<(*it).y()<<" to: "<<(*it).y() + startOffset<<endl;
	    m_doc->renderer()->mltMoveClip((*it).x(), (*it).x() + trackOffset, (*it).y(), (*it).y() + startOffset);

	    // Move clip transitions
	    TransitionStack stack = (*it)->clipTransitions();
	    TransitionStack::iterator itt = stack.begin();
    	    while (itt) {
		m_doc->renderer()->mltMoveTransition((*itt)->transitionTag(), (*itt)->transitionEndTrack(), (*itt)->transitionStartTrack(), (*itt)->transitionStartTime(), (*itt)->transitionEndTime(), (*itt)->transitionStartTime() + startOffset, (*itt)->transitionEndTime() + startOffset);
        	++itt;
    	    }
	    ct++;
        }*/
	//m_doc->renderer()->mltMoveClip(startTrack, endTrack, m_startTime, m_endTime);
	m_doc->moveSelectedClips(m_endTime - m_startTime, m_endTrack - m_startTrack);
    }

/** Unexecute this command */
    void KMoveClipsCommand::unexecute() {
	int startTrack = m_doc->projectClip().playlistTrackNum(m_endTrack);
	int endTrack = m_doc->projectClip().playlistTrackNum(m_startTrack);

	int trackOffset = endTrack - startTrack;
	int startOffset = (m_startTime - m_endTime).frames(m_doc->framesPerSecond());

	QPtrListIterator < DocClipRef > itt(m_clipList);
    	while (itt.current() != 0) {
	    int track = itt.current()->playlistTrackNum();
	    int start = itt.current()->trackStart().frames(m_doc->framesPerSecond());
  	    m_doc->renderer()->mltMoveClip(track, track + trackOffset, start, start + startOffset);

	    // Move clip transitions
	    TransitionStack stack = itt.current()->clipTransitions();
	    TransitionStack::iterator it = stack.begin();
    	    while (it) {
		m_doc->renderer()->mltMoveTransition((*it)->transitionTag(), (*it)->transitionStartTrack(), trackOffset, (*it)->transitionStartTime(), (*it)->transitionEndTime(), (*it)->transitionStartTime() - m_endTime + m_startTime, (*it)->transitionEndTime() - m_endTime + m_startTime);
        	++it;
    	    }
            ++itt;
        }

	m_doc->moveSelectedClips(m_startTime - m_endTime, m_startTrack - m_endTrack);
    }

}				// namespace Command
