/***************************************************************************
                          trackpanelcliprollfunction  -  description
                             -------------------
    begin                : Wed Aug 25 2004
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
#ifndef TRACKPANELCLIPROLLFUNCTION_H
#define TRACKPANELCLIPROLLFUNCTION_H

#include "qcursor.h"

#include "kdenlive.h"
#include "snaptogrid.h"
#include "trackpanelfunction.h"
#include "krollcommand.h"

class QMouseEvent;
class DocTrackBase;
class KdenliveApp;
class KdenliveDoc;
class KTimeLine;
class KMMTimeLine;
//namespace Command {
//	class KRollCommand;
//}

/**
Abstract Base Class for track panel functionality decorators. This and it's
derived classes allow different behaviours to be added to panels as required.

@author Jason Wood
*/
class TrackPanelClipRollFunction : public TrackPanelFunction
{
	Q_OBJECT
public:
	TrackPanelClipRollFunction(KdenliveApp *app,
					KTimeLine *timeline,
					KdenliveDoc *document);

	virtual ~TrackPanelClipRollFunction();


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
	double getMinimumDrag() const;
signals: // Signals
	/**
	Emitted when an operation moves the clip crop start.
	*/
	void signalClipCropStartChanged(DocClipRef *);
	/**
	Emitted when an operation moves the clip crop end.
	*/
	void signalClipCropEndChanged(DocClipRef *);
private:
	enum ResizeState {None, Start, End};
	static const uint s_resizeTolerance;
	KdenliveApp *m_app;
	KTimeLine *m_timeline;
	KdenliveDoc *m_document;
	DocClipRef * m_clipUnderMouse;
	DocClipRef * m_clipBeforeMouse;
	DocClipRef * m_clipAfterMouse;
	ResizeState m_resizeState;
  	/** This command holds the roll edit information during a resize operation */
  	Command::KRollCommand * m_rollCommand;
	SnapToGrid m_snapToGrid;
	
	//const double minimumDrag;
	/** Returns true if the x,y position is over a clip (and therefore, the roll function applies) */
	//bool mouseApplies(const QPoint &pos) const;
	
	
	
};

#endif
