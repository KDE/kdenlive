/***************************************************************************
                          TrackPanelClipSlipFunction.h  -  description
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
#ifndef TRACKPANELCLIPSLIPFUNCTION_H
#define TRACKPANELCLIPSLIPFUNCTION_H

#include "qcursor.h"

#include "docclipreflist.h"
#include "kdenlive.h"
#include "snaptogrid.h"
#include "trackpanelfunction.h"
#include "kmoveclipscommand.h"

class QMouseEvent;
class KdenliveDoc;
class DocTrackBase;

namespace Gui {
    class KTimeLine;
    class KdenliveApp;
}
/**
Abstract Base Class for track panel functionality decorators. This and it's
derived classes allow different behaviours to be added to panels as required.

@author Jason Wood
*/ class TrackPanelClipSlipFunction:public TrackPanelFunction
{
  Q_OBJECT public:
    TrackPanelClipSlipFunction(Gui::KdenliveApp * app,
	Gui::KTimeLine * timeline, KdenliveDoc * document);

    virtual ~ TrackPanelClipSlipFunction();

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

	/**
	Process drag events
	*/

    virtual bool dragEntered(Gui::KTrackPanel * panel, QDragEnterEvent *);
    virtual bool dragMoved(Gui::KTrackPanel * panel, QDragMoveEvent *);
    virtual bool dragLeft(Gui::KTrackPanel * panel, QDragLeaveEvent *);
    virtual bool dragDropped(Gui::KTrackPanel * panel, QDropEvent *);

  private:
     Gui::KdenliveApp * m_app;
     Gui::KTimeLine * m_timeline;
    KdenliveDoc *m_document;
    DocClipRef *m_clipUnderMouse;
    bool m_dragging;

	/**
	This variable should be set to true if we have initiated a drag which
	is going to be moving, rather than adding, clips.

	set to false otherwise. The purpose of this variable is to prevent the
	selection group from being re-created on drag entry if we are only
	moving it - this prevents a copy of the clips from being created.
	*/
    bool m_startedClipMove;

	/**
	This list is used to group clips together when they are being dragged away from the
	timeline, or are being dragged onto the timeline. It gives a home to clips that have not yet
	been placed.
	*/
    DocClipRefList m_selection;

	/**
	This is the "master" Clip - the clip that is actively being dragged by the mouse.
	All other clips move in relation to the master clip.
	*/
    DocClipRef *m_masterClip;

	/**
	When dragging a clip, this is the time offset that should be applied to where
	the mouse cursor to find the beginning of the master clip.
	*/
    GenTime m_clipOffset;

	/** A snap to grid object used for calculating snap-to-grid calculations. */
    SnapToGrid m_snapToGrid;

	/** Moves all selected clips to a new position. The new start position is that for the master clip,
	 all other clips are moved in relation to it. Returns true on success, false on failure.*/
    bool moveSelectedClips(int newTrack, GenTime start);

	/** Adds a Clipgroup to the tracks in the timeline. It there are some currently selected clips and
	we add new clips with this method, the previously selected clips are dselected. */
    void addClipsToTracks(DocClipRefList & clips, int track, GenTime value,
	bool selected);

	/** set up the snap-to-grid class */
    void setupSnapToGrid();

	/** Find the index of the document track underneath the specified point on the track. */
    int trackUnderPoint(const QPoint & pos);

	/** Initiates a drag operation on the selected clip, setting the master clip to clipUnderMouse,
	and specifying the time that the mouse is currently pointing at. */
    void initiateDrag(DocClipRef * clipUnderMouse, GenTime mouseTime);

	/**
	True if we are currently in the process of adding clips to the timeline.
	False otherwise.
	*/
    bool m_addingClips;


	/**
	A moveClipCommand action, used to record clip movement for undo/redo functionality.
	*/
     Command::KMoveClipsCommand * m_moveClipsCommand;
	/**
	This command is used to record clip deletion for undo/redo functionality.
	*/
    KMacroCommand *m_deleteClipsCommand;

#warning - The following method is a bad example for programming design.
	/** Returns a command that would create those clips in the document that are currently selected.
	*/
    KMacroCommand *createAddClipsCommand();

	/** Returns true if the x,y position is over a clip (and therefore, the move function applies) */
    bool mouseApplies(const QPoint & pos) const;
};

#endif
