/***************************************************************************
                          kmmruler.h  -  description
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

#ifndef KMMRULER_H
#define KMMRULER_H

#include <qwidget.h>

/**The multimedia ruler used in the timeline. The ruler handles various time formats, as well as
	* the various markers that are required.
  *@author Jason Wood
  */

enum KMMTimeScale  {KTS_HMSX,			// hours, minutes, seconds.hundredths
		   							KTS_HMSF,			// Hours, minutes, seconds, frames
										KTS_FRAME};		// Number of Frames

class KMMRuler : public QWidget {
   Q_OBJECT
public: 
	KMMRuler(QWidget *parent=0, const char *name=0);
	~KMMRuler();
  void paintEvent(QPaintEvent *event);
  QSize sizeHint();
  QString getTimeScaleText(int frame);
	int getTickCount(double size);						// calculates the step between ticks to make sure they are at least size pixels apart.
private:	
	KMMTimeScale timeScale;
	QSize _sizehint;	
	int textEvery;												// how many frames should be between each displayed value.	
	int smallTickEvery;										// how many frames should be between each big tick.
	int bigTickEvery;											// how many frames should be between each small tick.
	double xFrameSize;										// Size of each frame on the screen. Measured in pixels.
	int leftMostPixel;										// the leftmost pixel that the timeline should draw.
  int framesPerSecond;									// How many frames per second
public slots: // Public slots
  void setValue(int value);
  void setFrameSize(double size);
};

#endif
