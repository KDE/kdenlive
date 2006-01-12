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
#include <kdebug.h>

EffectParameter::EffectParameter(const QString &name) :
				m_name(name),
				m_selectedKeyFrame(0)
{
}


EffectParameter::~EffectParameter()
{
}

const int EffectParameter::numKeyFrames() const
{
	return m_keyFrames.count();
}

void EffectParameter::setSelectedKeyFrame(int ix)
{
	m_selectedKeyFrame = ix;
}

const int EffectParameter::selectedKeyFrame() const
{
	return m_selectedKeyFrame;
}

uint EffectParameter::addKeyFrame(EffectKeyFrame *effectKeyFrame)
{
	KeyFrameListIterator itt(m_keyFrames);
	double time = effectKeyFrame->time();
	uint i;
	for( i = 0; itt.current(); i++) {
		if (itt.current()->time() > time) break;
		++itt;
	}
	m_keyFrames.insert(i, effectKeyFrame);
	return i;
}

EffectKeyFrame *EffectParameter::keyframe(int ix) const
{
	KeyFrameListIterator itt(m_keyFrames);

	for(int count=0; itt.current() && count != ix; ++count) {
		++itt;
	}

	return itt.current();
}

EffectKeyFrame *EffectParameter::deleteKeyFrame(int ix)
{
	kdDebug()<<"EFFET++++++ remove item: "<<ix<<endl;
	m_keyFrames.remove(ix);
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
