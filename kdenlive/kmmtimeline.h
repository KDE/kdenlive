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
#include "clipgroup.h"

class QHBox;
class KdenliveDoc;
class QScrollView;
class QScrollBar;
class KMMTimeLineTrackView;
class KScalableRuler;

/**This is the timeline. It gets populated by tracks, which in turn are populated
by video and audio clips, or transitional clips, or any other clip imaginable.
  *@author Jason Wood
  */

class KMMTimeLine : public QVBox  {
   Q_OBJECT
public: 
	KMMTimeLine(QWidget *rulerToolWidget, QWidget *scrollToolWidget, KdenliveDoc *document, QWidget *parent=0, const char *name=0);
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
		
	  /** A pointer to the document (project) that this timeline is based upon */
	  KdenliveDoc * m_document;		
	  /** The track view area is the area under the ruler where tracks are displayed. */
	  KMMTimeLineTrackView *m_trackViewArea;
	  /** Holds a list of all selected clips, making it easier to perform manipulations on them. */
	  ClipGroup m_selection;
  /** This variable should be set to true if we have initiated a drag which
is going to be moving, rather than adding, clips.

set to false otherwise. The purpose of this variable is to prevent the
selection group from being re-created on drag entry if we are only
moving it - this prevents a copy of the clips from being created. */
  bool m_startedClipMove;
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
value that should be represented at that position. By using this, there is no need to calculate scale factors yourself. Takes the x
coordinate, and returns the value associated with it. */
  double mapLocalToValue(double coordinate) const;
  /** Deselects all clips on the timeline. */
  void selectNone();
  /** Returns m_trackList

Warning - this method is a bit of a hack, not good OO practice, and should be removed at some point. */
  QPtrList<KMMTrackPanel> &trackList();
  /** Moves all selected clips to a new position. The new start position is that for the master clip, all other clips are moved in relation to it. */
  void moveSelectedClips(int track, int start);
  /** Scrolls the track view area right by whatever the step value in the 
relevant scrollbar is. */
  void scrollViewRight();
  /** Scrolls the track view area left by whatever the step value of the relevant scroll bar is. */
  void scrollViewLeft();
  /** Selects the clip on the given track at the given value. */
  void selectClipAt(DocTrackBase &track, int value);
  /** Toggle Selects the clip on the given track and at the given value. The clip will become selected if it wasn't already selected, and will be deselected if it is. */
  void toggleSelectClipAt(DocTrackBase &track, int value);
  /** Returns true if the clip is selected, false otherwise. */
  bool clipSelected(DocClipBase *clip);
  /** Initiates a drag operation on the selected clip, setting the master clip to clipUnderMouse, and the x offset to clipOffset. */
  void initiateDrag(DocClipBase *clipUnderMouse, double clipOffset);
private: // private methods
	void resizeTracks();
  /** Adds a Clipgroup to the tracks in the timeline. */
	void addGroupToTracks(ClipGroup &group, int track, int value);
  /** Returns the integer value of the track underneath the mouse cursor. 
The track number is that in the track list of the document, which is
sorted incrementally from top to bottom. i.e. track 0 is topmost, track 1 is next down, etc. */
  int trackUnderPoint(const QPoint &pos);
public slots: // Public slots
  /** Called when a track within the project has been added or removed.
    *
		* The timeline needs to be updated to show these changes. */
  void syncWithDocument();
  
  /** Update the back buffer for the track views, and tell the trackViewArea widget to repaint itself. */
  void drawTrackViewBackBuffer();
  /** Sets a new time scale for the timeline. This in turn calls the correct kruler funtion and updates the display. */
  void setTimeScale(int scale);
public: // Public attributes
  /** When dragging a clip, this is the x offset that should be applied to the mouse cursor to find the beginning of the master clip. */
  double m_clipOffset;
};

#endif
