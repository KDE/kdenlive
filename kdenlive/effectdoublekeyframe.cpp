/***************************************************************************
                          effectdoublekeyframe  -  description
                             -------------------
    begin                : Fri Jan 2 2004
    copyright            : (C) 2004 by Jason Wood
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
#include "effectdoublekeyframe.h"
#include "assert.h"

EffectDoubleKeyFrame::EffectDoubleKeyFrame()
 : EffectKeyFrame()
{
}

EffectDoubleKeyFrame::EffectDoubleKeyFrame(double time, double value)  :
			EffectKeyFrame(time),
			m_value(value)
{
}


EffectDoubleKeyFrame::~EffectDoubleKeyFrame()
{
}

double EffectDoubleKeyFrame::value() const
{
	return m_value;
}

EffectKeyFrame *EffectDoubleKeyFrame::interpolateKeyFrame(EffectKeyFrame *keyframe, double time) const
{
	EffectKeyFrame *result = 0;

	double startTime = this->time();
	double endTime = keyframe->time();
	double startValue = this->value();

	assert(keyframe);
	assert(keyframe->toDoubleKeyFrame());
	assert(time >= startTime);
	assert(time <= endTime);

	EffectDoubleKeyFrame *doubleKeyFrame = keyframe->toDoubleKeyFrame();
	if(doubleKeyFrame) {
		double endValue = doubleKeyFrame->value();
		double value = startValue + ((endValue-startValue) * (time - startTime))/(endTime - startTime);
		result = new EffectDoubleKeyFrame(time, value);
	}

	return result;
}

EffectKeyFrame *EffectDoubleKeyFrame::clone() const
{
	return new EffectDoubleKeyFrame(time(), value());
}
