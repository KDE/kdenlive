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

#include <qwidget.h>
#include <qvbox.h>
#include <qlabel.h>
#include <qscrollbar.h>
#include <qscrollview.h>

#include <qpushbutton.h>

#include <kmmruler.h>

/**This is the timeline. It gets populated by tracks, which in turn are populated
by video and audio clips, or transitional clips, or any other clip imaginable.
  *@author Jason Wood
  */

class KMMTimeLine : public QVBox  {
   Q_OBJECT
public: 
	KMMTimeLine(QWidget *parent=0, const char *name=0);
	~KMMTimeLine();
private:
		/** GUI elements */
		QHBox rulerBox;				 	// Horizontal box holding the ruler
		QScrollView trackView; 	// Scrollview holding the trackBox
		QVBox trackBox;				 	// trackBox holding the tracks
		QHBox scrollBox;			 	// Horizontal box holding the horizontal scrollbar.
		QLabel trackLabel;
		KMMRuler ruler;
		QLabel scrollLabel;			// appears to the left of the bottom scroll bar.		
		QScrollBar scrollBar;		// this scroll bar's movement is measured in pixels, not frames.		
};

#endif
