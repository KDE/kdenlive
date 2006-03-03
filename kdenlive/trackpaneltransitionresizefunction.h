/***************************************************************************
                          TrackPanelTransitionResizeFunction.h  -  description
                             -------------------
    begin                : Sun Feb 9 2006
    copyright            : (C) 2006 Jean-Baptiste Mardelle, jb@ader.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef TRACKPANELTRANSITIONRESIZEFUNCTION_H
#define TRACKPANELTRANSITIONRESIZEFUNCTION_H

#include "qcursor.h"

#include "kdenlive.h"
#include "snaptogrid.h"
#include "trackpanelfunction.h"
#include "transition.h"

class QMouseEvent;

class DocTrackBase;
class KdenliveDoc;

namespace Gui {
    class KdenliveApp;
    class KTimeLine;
} namespace Command {
    class KResizeCommand;
}

class TrackPanelTransitionResizeFunction:public TrackPanelFunction
{
  Q_OBJECT public:
    TrackPanelTransitionResizeFunction(Gui::KdenliveApp * app,
	Gui::KTimeLine * timeline, KdenliveDoc * document);

    virtual ~ TrackPanelTransitionResizeFunction();


	/**
	Returns true if the specified position should cause this function to activate,
	otherwise returns false.
	*/
    virtual bool mouseApplies(Gui::KTrackPanel * panel,
	QMouseEvent * event) const;

	/**
	Returns a relevant mouse cursor for the given mouse position
	*/
    virtual QCursor getMouseCursor(Gui::KTrackPanel * panel,
	QMouseEvent * event);

	/**
	A mouse button has been pressed. Returns true if we want to handle this event
	*/
    virtual bool mousePressed(Gui::KTrackPanel * panel,
	QMouseEvent * event);

	/** Processes Mouse double click.*/
    virtual bool mouseDoubleClicked(Gui::KTrackPanel * panel, QMouseEvent * event);


	/**
	Mouse Release Events in the track view area. Returns true if we have finished
	an operation now.
	*/
    virtual bool mouseReleased(Gui::KTrackPanel * panel,
	QMouseEvent * event);

	/**
	Processes Mouse Move events in the track view area. Returns true if we are
	continuing with the drag.*/
    virtual bool mouseMoved(Gui::KTrackPanel * panel, QMouseEvent * event);

     signals:			// Signals
  /**
  Emitted when a keyframe was changed.
  */
    void redrawTrack();
    void transitionChanged(bool);;

  private:
    enum ResizeState { None, Start, End };
    static const uint s_resizeTolerance;
    Gui::KdenliveApp * m_app;
    Gui::KTimeLine * m_timeline;
    KdenliveDoc *m_document;
    DocClipRef *m_clipUnderMouse;
    ResizeState m_resizeState;
    Transition *m_selectedTransition;
    
	/** This command holds the resize information during a resize operation */
     Command::KResizeCommand * m_resizeCommand;
    SnapToGrid m_snapToGrid;
    bool m_refresh;
};

#endif
