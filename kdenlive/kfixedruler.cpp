/***************************************************************************
                          kfixedruler.cpp  -  description
                             -------------------
    begin                : Sun Jul 7 2002
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

#include "kfixedruler.h"
#include <kdebug.h>

KFixedRuler::KFixedRuler(int startValue, int endValue, int margin, 
			KRulerModel *model, QWidget *parent, const char *name ) :
								KRuler(model, parent, name)
{
	m_margin = margin;
	setRange(startValue, endValue);

	connect(this, SIGNAL(resized()), this, SLOT(calculateViewport()));
}

KFixedRuler::KFixedRuler(KRulerModel *model, QWidget *parent, const char *name) :
								KRuler(model, parent, name)
{
	m_margin = 20;
	setRange(0, 60);
	connect(this, SIGNAL(resized()), this, SLOT(calculateViewport()));
}

KFixedRuler::KFixedRuler(QWidget *parent, const char *name) :
								KRuler(parent, name)
{
	m_margin = 20;
	setRange(0, 60);
	connect(this, SIGNAL(resized()), this, SLOT(calculateViewport()));	
}

KFixedRuler::~KFixedRuler()
{
}

/** Calculates the required values to ensure that startValue is at the start of the ruler, and endValue is at the end of it. */
void KFixedRuler::calculateViewport()
{
	int numValues = maxValue() - minValue();
	double scale = (((double)width()-(2*margin())) / (double)numValues);
	int firstPixel = (int)(minValue()*scale) - margin();

	setStartPixel(firstPixel);
	setValueScale(scale);
}

/** Sets the start value for the ruler - that is, the leftmost value that should be displayed. */
void KFixedRuler::setMinValue(const int start)
{
	setRange(start, maxValue());
}

/** Sets the end value of the ruler - that is the last value which should be displayed on the ruler. */
void KFixedRuler::setMaxValue(const int end)
{
	setRange(minValue(), end);
}

/** Sets the range of values which are displayed on the timeline. */
void KFixedRuler::setRange(const int min, const int max)
{
	bool hasChanged = false;
	
	if(minValue()!=min) {
		hasChanged = true;
		emit startValueChanged(min);		
	}
	
	if(maxValue() != max) {
		hasChanged = true;
		emit endValueChanged(max);
	}

	KRuler::setRange(min, max);
	
	if(hasChanged) {
		calculateViewport();
	}	
}

/** Returns the minimum value of this ruler */
int KFixedRuler::minValue() const
{
	return KRuler::minValue();
}

/** Returns the maximum value of this ruler. */
int KFixedRuler::maxValue() const
{
	return KRuler::maxValue();
}

/** Sets the margin on either side of the ruler. This determines how much "out of range" ruler is displayed on screen. Useful for tidying up the ruler. */
void KFixedRuler::setMargin(const int margin)
{
	m_margin = margin;
	calculateViewport();
}

/** Returns the current margin of the ruler */
int KFixedRuler::margin() const
{
	return m_margin;
}
