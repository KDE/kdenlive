/***************************************************************************
                          kmmtrackpanel.h  -  description
                             -------------------
    begin                : Tue Aug 6 2002
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

#ifndef KMMTRACKPANEL_H
#define KMMTRACKPANEL_H

#include <qpixmap.h>
#include <qpainter.h>
#include <qhbox.h>
#include <qcursor.h>

#include "doctrackbase.h"

class KMMTimeLine;

/**Base class for all Track panels and their associated views.
  *@author Jason Wood
  */

class KMMTrackPanel : public QHBox  {
	Q_OBJECT
public:
	enum ResizeState {None, Start, End};
  
	KMMTrackPanel(KMMTimeLine *timeline, DocTrackBase *docTrack, QWidget *parent, const char *name);
	~KMMTrackPanel();
  /** Read property of KMMTimeLine * m_timeline. */
  KMMTimeLine * timeLine();
  /** returns the document track which is displayed by this track */
  DocTrackBase * docTrack();
  /** This function will paint a clip on screen. This funtion must be provided by a derived class. */
	virtual void paintClip(QPainter &painter, DocClipBase *clip, QRect &rect, bool selected) = 0;
	/** Paints the backbuffer into the relevant place using the painter supplied. The track should be drawn into
	the area provided in area */
	void drawToBackBuffer(QPainter &painter, QRect &rect);	
  /** A mouse button has been pressed. Returns true if we want to handle this event */
  virtual bool mousePressed(QMouseEvent *event) = 0;
  /** Mouse Release Events in the track view area. Returns true if we have finished an operation now.*/
  virtual bool mouseReleased(QMouseEvent *event) = 0;
  /** Processes Mouse Move events in the track view area. Returns true if we are continuing with the drag.*/
  virtual bool mouseMoved(QMouseEvent *event) = 0;
  /** Set the mouse cursor to a relevant shape, depending on it's location within the track view area.
  	 The location is obtained from event. */
  virtual QCursor getMouseCursor(QMouseEvent *event) = 0;

   /** This value specifies the resizeTolerance of the KMMTimeLine - that is, how many
pixels at the start and end of a clip are considered as a resize operation. */
  static int resizeTolerance;  
protected: // Protected attributes
  /** The track document class that should be queried to build up this track view. */
  DocTrackBase *m_docTrack;
  /** The KMMTrackPanel needs access to various methods from it's parents Timeline. The parent timeline
  	 is stored in this variable. */
  KMMTimeLine *m_timeline;
signals: // Signals
  /** Emitted when an operation moves the clip crop start. */
  void signalClipCropStartChanged(const GenTime &);
  /** Emitted when an operation moves the clip crop end. */
  void signalClipCropEndChanged(const GenTime &);  
  /** emitted when a tool is "looking" at a clip, it signifies to whatever is listening that displaying this information in some way would be useful. */
  void lookingAtClip(DocClipBase *, const GenTime &);
};

#endif
