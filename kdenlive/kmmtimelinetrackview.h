/***************************************************************************
                          kmmtimelinetrackview.h  -  description
                             -------------------
    begin                : Wed Aug 7 2002
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

#ifndef KMMTIMELINETRACKVIEW_H
#define KMMTIMELINETRACKVIEW_H

#include <qwidget.h>
#include <qpixmap.h>
#include "gentime.h"

class KMMTimeLine;
class DocClipBase;
class KMMTrackPanel;

/**Timeline track view area.
  *@author Jason Wood
  */

class KMMTimeLineTrackView : public QWidget  {
public:
	enum ResizeState {None, Start, End};
	KMMTimeLineTrackView(KMMTimeLine &timeLine, QWidget *parent=0, const char *name=0);
	~KMMTimeLineTrackView();
	void drawBackBuffer();
	void resizeEvent(QResizeEvent *event);
	void paintEvent(QPaintEvent *event);
	/** This event occurs when the mouse has been moved. */
  void mouseMoveEvent(QMouseEvent *event);
  /** This event occurs when a mouse button is released. */
  void mouseReleaseEvent(QMouseEvent *event);
  /** This event occurs when a mouse button is pressed. */
  void mousePressEvent(QMouseEvent *event);
private: // Private attributes
  QPixmap m_backBuffer;
  KMMTimeLine & m_timeLine;

  int m_trackBaseNum;
  DocClipBase *m_clipUnderMouse;  
  /** The value offset between the mouse position and the start of the master clip. */
  GenTime m_clipOffset;
  /** During a resize operation, holds the current resize state, as defined in the ResizeState enum. */
  ResizeState m_resizeState;
  KMMTrackPanel * m_panelUnderMouse;
};

#endif
