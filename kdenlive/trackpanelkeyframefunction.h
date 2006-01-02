/***************************************************************************
                          trackpanelclipresizefunction.h  -  description
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
#ifndef TRACKPANELKEYFRAMEFUNCTION_H
#define TRACKPANELKEYFRAMEFUNCTION_H

#include "qcursor.h"

#include "kdenlive.h"
#include "snaptogrid.h"
#include "trackpanelfunction.h"

class QMouseEvent;

class DocTrackBase;
class KdenliveDoc;

namespace Gui
{
	class KdenliveApp;
	class KTimeLine;
}


namespace Command {
	class KResizeCommand;
}

/**
Abstract Base Class for track panel functionality decorators. This and it's
derived classes allow different behaviours to be added to panels as required.

@author Jason Wood
*/
class TrackPanelKeyFrameFunction : public TrackPanelFunction
{
	Q_OBJECT
public:
	TrackPanelKeyFrameFunction(Gui::KdenliveApp *app,
					Gui::KTimeLine *timeline,
					KdenliveDoc *document);

	virtual ~TrackPanelKeyFrameFunction();


	/**
	Returns true if the specified position should cause this function to activate,
	otherwise returns false.
	*/
	virtual bool mouseApplies(Gui::KTrackPanel *panel, QMouseEvent *event) const;

	/**
	Returns a relevant mouse cursor for the given mouse position
	*/
	virtual QCursor getMouseCursor(Gui::KTrackPanel *panel, QMouseEvent *event);

	/**
	A mouse button has been pressed. Returns true if we want to handle this event
	*/
	virtual bool mousePressed(Gui::KTrackPanel *panel, QMouseEvent *event);

	/**
	Mouse Release Events in the track view area. Returns true if we have finished
	an operation now.
	*/
	virtual bool mouseReleased(Gui::KTrackPanel *panel, QMouseEvent *event);

	/**
	Processes Mouse Move events in the track view area. Returns true if we are
	continuing with the drag.*/
	virtual bool mouseMoved(Gui::KTrackPanel *panel, QMouseEvent *event);


	void setKeyFrame(int i);
signals: // Signals
  /**
  Emitted when a keyframe was changed.
  */
  void signalKeyFrameChanged(bool); //DocClipRef *);

  void redrawTrack();

private:
	//enum ActionState {Move, Create, None};
	static const uint s_resizeTolerance;
	Gui::KdenliveApp *m_app;
	Gui::KTimeLine *m_timeline;
	KdenliveDoc *m_document;
	DocClipRef * m_clipUnderMouse;
	//ActionState m_actionState;
	int m_selectedKeyframe;
	int m_selectedKeyframeValue;
  	/** This command holds the resize information during a resize operation */
  	Command::KResizeCommand * m_resizeCommand;
	SnapToGrid m_snapToGrid;
};

#endif
