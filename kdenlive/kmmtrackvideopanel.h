/***************************************************************************
                          kmmtrackvideopanel.h  -  description
                             -------------------
    begin                : Tue Apr 9 2002
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

#ifndef KMMTRACKVIDEOPANEL_H
#define KMMTRACKVIDEOPANEL_H

#include <qwidget.h>
#include <qlabel.h>

#include "doctrackvideo.h"
#include "kmmtrackpanel.h"

class KResizeCommand;

/**KMMTrackVideoPanel contains useful controls for manipulating the video tracks
which reside in the main video widget
  *@author Jason Wood  
  */

class KMMTrackVideoPanel : public KMMTrackPanel  {
   Q_OBJECT
public:
	enum ResizeState {None, Start, End};
	KMMTrackVideoPanel(KMMTimeLine &timeline, DocTrackVideo & docTrack, QWidget *parent=0, const char *name=0);
	~KMMTrackVideoPanel();
  /** Paint the specified clip on screen within the specified rectangle, using the specified painter. */
  void paintClip(QPainter &painter, DocClipBase *clip, QRect &rect, bool selected);
  /** A mouse press event has occured. Perform relevant mouse operations. Return true if we are going to be watching
			for other mouse press events */
  bool mousePressed(QMouseEvent *event);
  /** A mouse button has been released. Returns true if we have finished whatever
  		operation we were performing, false otherwise. */
	bool mouseReleased(QMouseEvent *event);
  /** Set the cursor to an appropriate shape, relative to the position on the track. */
  QCursor getMouseCursor(QMouseEvent *event);
  /** The mouse has been moved (whilst we are "dragging") on this track. Performs any
  operations that should be performed. */
  bool mouseMoved(QMouseEvent *event);
public: // public attributes
   /** This value specifies the resizeTolerance of the KMMTimeLine - that is, how many
pixels at the start and end of a clip are considered as a resize operation. */
  static int resizeTolerance;
private:
	QLabel m_trackLabel;
  /** During a resize operation, holds the current resize state, as defined in the ResizeState enum. */
  ResizeState m_resizeState;	
  /** The clip that is under the mouse at present */
  DocClipBase * m_clipUnderMouse;  
  /** True if we are inside a dragging operation, false otherwise. */
  bool m_dragging;;
  /** This command holds the resize information during a resize operation */
  KResizeCommand * m_resizeCommand;
};

#endif
