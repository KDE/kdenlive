/***************************************************************************
                          krulersliderbase.h  -  description
                             -------------------
    begin                : Sat Jul 20 2002
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

#ifndef KRULERSLIDERBASE_H
#define KRULERSLIDERBASE_H

class QPainter;

/**This is the base class for all kruler sliders. A "slider" is any marker which appears on the kruler. Each slider has a number of properties, including value and current state.

	The KRulerSliderBase class is NOT a QWidget. It contains a number of virtual functions for drawing,
	detecting when the mouse is within the slider, and for suggesting the bounding box for the slider.
  *@author Jason Wood
  */

class KRulerSliderBase {
  public:
    KRulerSliderBase();
    virtual ~ KRulerSliderBase();
  /** Draws a horizontal version of the slider, using the painter provided. The "x" coordinate is the pixel which coresponds to the slider value. The height of the widget is passed so that the marker can rescale itself to an appropriate size. */
    virtual void drawHorizontalSlider(QPainter & painter, const int x,
	const int height) = 0;

/** Returns true if the currrent slider is under the mouse position passed, otherwise returns false.
midx is the coordinate corresponding to the value of the slider. height is that of the widget, as is passed so that the slider can determine it's scale. The x,y pair is the point that we are testing. */
    virtual bool underMouse(const int x, const int y, const int midx,
	const int height) const = 0;
  /** Returns a new object of the same class as this one. */
    KRulerSliderBase *newInstance();
  public:			// Public attributes
  /** reference counter */
    unsigned int m_ref;
  /** Decreases reference count by one. If this count reaches zero, nothing is refering to the class and it can be deleted. */
    void decrementRef();
  /** Add one to the reference count. If this count reaches zero, the KRulerSliderBase is destroyed. */
    void incrementRef();
  /** Returns the right-most pixel that will be drawn by this slider in it's current position on the timeline. */
    virtual int rightBound(int x, int height) = 0;
  /** Returns the left-most pixel on the timeline that will be drawn by this slider in
	it's current position. */
    virtual int leftBound(int x, int height) = 0;
};

#endif
