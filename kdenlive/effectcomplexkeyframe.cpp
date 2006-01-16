/***************************************************************************
                          effectcomplexkeyframe  -  description
                             -------------------
    begin                : Fri Jan 16 2006
    copyright            : (C) 2006 by Jean-Baptiste Mardelle
    email                : jb@ader.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qstringlist.h>

#include "effectcomplexkeyframe.h"
#include "assert.h"

EffectComplexKeyFrame::EffectComplexKeyFrame()
:  EffectKeyFrame()
{
}

EffectComplexKeyFrame::EffectComplexKeyFrame(double time, QStringList values):
EffectKeyFrame(time), m_values(values)
{
}


EffectComplexKeyFrame::~EffectComplexKeyFrame()
{
}

void EffectComplexKeyFrame::setValue(int ix, QString values)
{
    m_values[ix] = values;
}

int EffectComplexKeyFrame::value(int ix) const
{
    return m_values[ix].toInt();
}

const QString EffectComplexKeyFrame::processComplexKeyFrame() const
{
    return m_values[0] + "," + m_values[1] + ":" + m_values[2] + "x" +
	m_values[3] + ":" + m_values[4] + "x" + m_values[4];
}


EffectKeyFrame *EffectComplexKeyFrame::interpolateKeyFrame(EffectKeyFrame *
    keyframe, double time) const
{
/*	EffectKeyFrame *result = 0;

	double startTime = this->time();
	double endTime = keyframe->time();
	double startValue = this->value();

	assert(keyframe);
	assert(keyframe->toDoubleKeyFrame());
	assert(time >= startTime);
	assert(time <= endTime);

	EffectComplexKeyFrame *doubleKeyFrame = keyframe->toDoubleKeyFrame();
	if(doubleKeyFrame) {
		double endValue = doubleKeyFrame->value();
		double value = startValue + ((endValue-startValue) * (time - startTime))/(endTime - startTime);
		result = new EffectComplexKeyFrame(time, value);
	}

	return result;*/
}

EffectKeyFrame *EffectComplexKeyFrame::clone() const
{
    return new EffectComplexKeyFrame(time(), m_values);
}
