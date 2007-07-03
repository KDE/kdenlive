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
#ifndef TRACKPANELCLIPRESIZEFUNCTION_H
#define TRACKPANELCLIPRESIZEFUNCTION_H

#include "qcursor.h"

#include "kdenlive.h"
#include "snaptogrid.h"
#include "trackpanelfunction.h"

class QMouseEvent;

class DocTrackBase;
class KdenliveDoc;

namespace Gui {
    class KdenliveApp;
    class KTimeLine;
} namespace Command {
    class KResizeCommand;
}
/**
Abstract Base Class for track panel functionality decorators. This and it's
derived classes allow different behaviours to be added to panels as required.

@author Jason Wood
*/ class TrackPanelClipResizeFunction:public TrackPanelFunction
{
  Q_OBJECT public:
    TrackPanelClipResizeFunction(Gui::KdenliveApp * app,
	Gui::KTimeLine * timeline, KdenliveDoc * document);

    virtual ~ TrackPanelClipResizeFunction();


	/**
	Returns true if the specified position should cause this function to activate,
	otherwise returns false.
	*/
    virtual bool mouseApplies(Gui::KTrackPanel * panel,
	QMouseEvent * event) const;

	/**
	Returns a relevant mouse cursor for the given mouse position
	*/
    virtual QCursor getMouseCursor(Gui::KTrackPanel *, QMouseEvent *);

	/**
	A mouse button has been pressed. Returns true if we want to handle this event
	*/
    virtual bool mousePressed(Gui::KTrackPanel * panel,
	QMouseEvent * event);

	/** Processes Mouse double click.*/
    virtual bool mouseDoubleClicked(Gui::KTrackPanel *, QMouseEvent *);

	/**
	Mouse Release Events in the track view area. Returns true if we have finished
	an operation now.
	*/
    virtual bool mouseReleased(Gui::KTrackPanel *, QMouseEvent *);

	/**
	Processes Mouse Move events in the track view area. Returns true if we are
	continuing with the drag.*/
    virtual bool mouseMoved(Gui::KTrackPanel * panel, QMouseEvent * event);
     signals:			// Signals
  /**
  Emitted when an operation moves the clip crop start.
  */
    void signalClipCropStartChanged(DocClipRef *);
  /**
  Emitted when an operation moves the clip crop end.
  */
    void signalClipCropEndChanged(DocClipRef *);
  private:
    enum ResizeState { None, Start, End };
    static const uint s_resizeTolerance;
     Gui::KdenliveApp * m_app;
     Gui::KTimeLine * m_timeline;
    KdenliveDoc *m_document;
    DocClipRef *m_clipUnderMouse;
    ResizeState m_resizeState;

    QCursor m_endCursor;
    QCursor m_startCursor;

	/** This command holds the resize information during a resize operation */
     Command::KResizeCommand * m_resizeCommand;
    SnapToGrid m_snapToGrid;
};

#endif
