/***************************************************************************
                          kmmtimeline.h  -  description
                             -------------------
    begin                : Fri Feb 15 2002
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

#ifndef KMMTIMELINE_H
#define KMMTIMELINE_H

#include <qlist.h>
#include <qvbox.h>

#include "kmmtrackpanel.h"

class QHBox;
class KdenliveDoc;
class QScrollView;
class QScrollBar;
class KMMTimeLineTrackView;
class KScalableRuler;
class KdenliveApp;

/**This is the timeline. It gets populated by tracks, which in turn are populated
by video and audio clips, or transitional clips, or any other clip imaginable.
  *@author Jason Wood
  */

class KMMTimeLine : public QVBox  {
   Q_OBJECT
public: 
	KMMTimeLine(KdenliveApp *app, QWidget *rulerToolWidget, QWidget *scrollToolWidget, KdenliveDoc *document, QWidget *parent=0, const char *name=0);
	~KMMTimeLine();
private:
	/** GUI elements */
	QHBox *m_rulerBox;				 	// Horizontal box holding the ruler
	QScrollView *m_trackScroll; 	// Scrollview holding the tracks
	QHBox *m_scrollBox;			 	// Horizontal box holding the horizontal scrollbar.
	QWidget *m_rulerToolWidget;	// This widget is supplied by the constructor, and appears to the left of the ruler.
	KScalableRuler *m_ruler;
	QWidget *m_scrollToolWidget; // This widget is supplied by the constructor and appears to the left of the bottom scrollbar.
	QScrollBar *m_scrollBar;		// this scroll bar's movement is measured in pixels, not frames.
	/** track varables */
	QPtrList<KMMTrackPanel> m_trackList;

	KdenliveApp *m_app;
		
  /** A pointer to the document (project) that this timeline is based upon */
  KdenliveDoc * m_document;
	  
  /** The track view area is the area under the ruler where tracks are displayed. */
  KMMTimeLineTrackView *m_trackViewArea;
	  
 	/** This variable should be set to true if we have initiated a drag which
	is going to be moving, rather than adding, clips.

	set to false otherwise. The purpose of this variable is to prevent the
	selection group from being re-created on drag entry if we are only
	moving it - this prevents a copy of the clips from being created. */
  bool m_startedClipMove;

	  
  /** This list is used to group clips together when they are being dragged away from the
  timeline, or are being dragged onto the timeline. It gives a home to clips that have not yet
  been placed. */
  DocClipBaseList m_selection;
  
  /** This is the "master" Clip - the clip that is actively being dragged by the mouse.
	All other clips move in relation to the master clip. */
  DocClipBase * m_masterClip;
  /** A list of all times that are "snapToGrid" times. Generated specifically for a
	particular mouse/offset and relative to the selected clips, this list of times is the
	only thing we need to consult when moving back and forth with the mouse to
	determine when we need to snap to a particular location based upon another
	clip. */
  QValueList<GenTime> m_snapToGridList;
  /** Keeps track of whichever list item is closest to the mouse cursor. */
  QValueListIterator<GenTime> m_gridSnapTracker;
  /** The snap tolerance specifies how many pixels away a selection is from a 
snap point before the snap takes effect. */
  static int snapTolerance;  
public: // Public methods
  /** This method adds a new track to the trackGrid. */
  void appendTrack(KMMTrackPanel *track);
  
  void resizeEvent(QResizeEvent *event);
  
  /** Inserts a track at the position specified by index */
  void insertTrack(int index, KMMTrackPanel *track);
  
  /** No descriptions */
  void polish();

	void dragEnterEvent ( QDragEnterEvent * );
	void dragMoveEvent ( QDragMoveEvent * );
	void dragLeaveEvent ( QDragLeaveEvent * );
	void dropEvent ( QDropEvent * );

  /** Takes the value that we wish to find the coordinate for, and returns the x coordinate. In cases where a single value covers multiple
	pixels, the left-most pixel is returned. */	
  double mapValueToLocal(double value) const;
  
  /** This method maps a local coordinate value to the corresponding
	value that should be represented at that position. By using this, there is no need to
	calculate scale factors yourself. Takes the x coordinate, and returns the value associated
	with it. */
  double mapLocalToValue(double coordinate) const;
  
  /** Deselects all clips on the timeline. This does not affect any clips that are "in transition" onto the
	timeline,i.e. in a drag process, clips that are in m_selection but have not been placed do not become
	deselected */
  void selectNone();
  
  /** Returns m_trackList

	Warning - this method is a bit of a hack, not good OOP practice, and should be removed at some point. */
  QPtrList<KMMTrackPanel> &trackList();
  
  /** Moves all selected clips to a new position. The new start position is that for the master clip,
   all other clips are moved in relation to it. Returns true on success, false on failure.*/
  bool moveSelectedClips(int newTrack, GenTime start);
  
  /** Scrolls the track view area right by whatever the step value in the 
	relevant scrollbar is. */
  void scrollViewRight();
  
  /** Scrolls the track view area left by whatever the step value of the relevant scroll bar is. */
  void scrollViewLeft();
  
  /** Selects the clip on the given track at the given value. */
  void selectClipAt(DocTrackBase &track, GenTime value);
  
  /** Toggle Selects the clip on the given track and at the given value. The clip will become
  selected if it wasn't already selected, and will be deselected if it is. */
  void toggleSelectClipAt(DocTrackBase &track, GenTime value);
  
  /** Initiates a drag operation on the selected clip, setting the master clip to clipUnderMouse,
  and the x offset to clipOffset. */
  void initiateDrag(DocClipBase *clipUnderMouse, GenTime clipOffset);
  
  /** Returns true if the specified clip exists and is selected, false otherwise. If a track is
  specified, we look at that track first, but fall back to a full search of tracks if the clip is
  not there. */
  bool clipSelected(DocClipBase *clip, DocTrackBase *track=0);
  /** Returns true if the specified cliplist can be successfully merged with the track
	views, false otherwise. */
  bool canAddClipsToTracks(DocClipBaseList &clips, int track, GenTime clipOffset);
  /** Returns the seek position of the timeline - this is the currently playing frame, or
the currently seeked frame. */
  GenTime seekPosition();
private: // private methods
	void resizeTracks();
	
  /** Adds a Clipgroup to the tracks in the timeline. It there are some currently selected clips and
  we add new clips with this method, the previously selected clips are dselected. */
	void addClipsToTracks(DocClipBaseList &clips, int track, GenTime value, bool selected);
	
  /** Returns the integer value of the track underneath the mouse cursor. 
	The track number is that in the track list of the document, which is
	sorted incrementally from top to bottom. i.e. track 0 is topmost, track 1 is next down, etc. */
  int trackUnderPoint(const QPoint &pos);
  /** Constructs a list of all clips that are currently selected. It does nothing else i.e.
it does not remove the clips from the timeline. */
  DocClipBaseList listSelected();
  /** Constructs the snap to grid list, in preperation for using it in a move operation. */
  void generateSnapToGridList();
  
public slots: // Public slots
  /** Called when a track within the project has been added or removed.
    *
		* The timeline needs to be updated to show these changes. */
  void syncWithDocument();
  
  /** Update the back buffer for the track views, and tell the trackViewArea widget to
  repaint itself. */
  void drawTrackViewBackBuffer();
  
  /** Sets a new time scale for the timeline. This in turn calls the correct kruler funtion and
  updates the display. */
  void setTimeScale(int scale);
  /** Calculates the size of the project, and sets up the timeline to accomodate it. */
  void calculateProjectSize(double rulerScale);
  
public: // Public attributes
  /** When dragging a clip, this is the time offset that should be applied to where the mouse cursor
  to find the beginning of the master clip. */
  GenTime m_clipOffset;
signals: // Signals
  /** emitted when the length of the project has changed. */
  void projectLengthChanged(int numFrames);
};

#endif
