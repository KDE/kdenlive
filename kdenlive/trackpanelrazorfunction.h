/***************************************************************************
                          trackpanelfunction.h  -  description
                             -------------------
    begin                : Sun May 18 2003
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
#ifndef TRACKPANELRAZORFUNCTION_H
#define TRACKPANELRAZORFUNCTION_H

#include "kdenlive.h"
#include "trackpanelfunction.h"

class KMMTimeLine;
class DocTrackBase;

/**
Abstract Base Class for track panel functionality decorators. This and it's
derived classes allow different behaviours to be added to panels as required.

@author Jason Wood
*/
class TrackPanelRazorFunction : public TrackPanelFunction
{
	Q_OBJECT
public:
	TrackPanelRazorFunction(KMMTimeLine *timeline, DocTrackBase *docTrack);

	virtual ~TrackPanelRazorFunction();

	/**
	Returns true if the specified position should cause this function to activate,
	otherwise returns false.
	*/
	virtual bool mouseApplies(QMouseEvent *event) const;

	/**
	Returns a relevant mouse cursor for the given mouse position
	*/
	virtual QCursor getMouseCursor(QMouseEvent *event);

	/**
	A mouse button has been pressed. Returns true if we want to handle this event
	*/
	virtual bool mousePressed(QMouseEvent *event);

	/**
	Mouse Release Events in the track view area. Returns true if we have finished
	an operation now.
	*/
	virtual bool mouseReleased(QMouseEvent *event);

	/**
	Processes Mouse Move events in the track view area. Returns true if we are
	continuing with the drag.*/
	virtual bool mouseMoved(QMouseEvent *event);
private:

	KMMTimeLine *m_timeline;
	DocTrackBase *m_docTrack;
	DocClipBase * m_clipUnderMouse;
};

#endif
