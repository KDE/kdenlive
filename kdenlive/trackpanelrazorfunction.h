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

class KdenliveApp;
class KdenliveDoc;
class KTimeLine;
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
	TrackPanelRazorFunction(KdenliveApp *app,
					KTimeLine *timeline,
					KdenliveDoc *document);

	virtual ~TrackPanelRazorFunction();

	/**
	Returns true if the specified position should cause this function to activate,
	otherwise returns false.
	*/
	virtual bool mouseApplies(KTrackPanel *panel, QMouseEvent *event) const;

	/**
	Returns a relevant mouse cursor for the given mouse position
	*/
	virtual QCursor getMouseCursor(KTrackPanel *panel, QMouseEvent *event);

	/**
	A mouse button has been pressed. Returns true if we want to handle this event
	*/
	virtual bool mousePressed(KTrackPanel *panel, QMouseEvent *event);

	/**
	Mouse Release Events in the track view area. Returns true if we have finished
	an operation now.
	*/
	virtual bool mouseReleased(KTrackPanel *panel, QMouseEvent *event);

	/**
	Processes Mouse Move events in the track view area. Returns true if we are
	continuing with the drag.*/
	virtual bool mouseMoved(KTrackPanel *panel, QMouseEvent *event);
signals: // Signals
	/**
	emitted when a tool is "looking" at a clip, it signifies to whatever is listening
	that displaying this information in some way would be useful.
	*/
	void lookingAtClip(DocClipRef *, const GenTime &);
private:

	KdenliveApp *m_app;
	KTimeLine *m_timeline;
	KdenliveDoc *m_document;
	DocClipRef * m_clipUnderMouse;
};

#endif
