/***************************************************************************
                          effectparameter  -  description
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
#include "effectparameter.h"
#include "effectkeyframe.h"

EffectParameter::EffectParameter(const QString &name) :
				m_name(name)
{
}


EffectParameter::~EffectParameter()
{
}

int EffectParameter::numKeyFrames() const
{
	return m_keyFrames.count();
}

EffectKeyFrame *EffectParameter::keyframe(int ix) const
{
	KeyFrameListIterator itt(m_keyFrames);

	for(int count=0; itt.current() && count != ix; ++count) {
		++itt;
	}

	return itt.current();
}

EffectKeyFrame *EffectParameter::interpolateKeyFrame(double time) const
{
	EffectKeyFrame *keyFrame = NULL;

	EffectKeyFrame *nextKeyFrame = NULL;
	EffectKeyFrame *prevKeyFrame = NULL;

	KeyFrameListIterator itt(m_keyFrames);
	while(itt.current()) {
		if(itt.current()->time() >= time) {
			nextKeyFrame = itt.current();
			--itt;
			prevKeyFrame = itt.current();
		}
		++itt;
	}

	if(nextKeyFrame && prevKeyFrame) {
		keyFrame = prevKeyFrame->interpolateKeyFrame(nextKeyFrame, time);
	} else if (nextKeyFrame) {
		keyFrame = nextKeyFrame->clone();
	} else if(prevKeyFrame) {
		keyFrame = prevKeyFrame->clone();
	}

	return keyFrame;
}
