/***************************************************************************
                         effectdesc.cpp  -  description
                            -------------------
   begin                : Sun Feb 9 2003
   copyright            : (C) 2003 by Jason Wood
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

#include <assert.h>
#include "kdebug.h"

#include "effectdesc.h"

#include "effect.h"
#include "effectparamdesc.h"

EffectDesc::EffectDesc( const QString &name ) :
		m_name( name )
{
	m_params.setAutoDelete(true);
}

EffectDesc::~EffectDesc()
{}

/** Returns the name of this effect. */
const QString &EffectDesc::name() const
{
	return m_name;
}

/** Adds an input to this description. An input might be a video stream, and audio stream, or it may require both. */
void EffectDesc::addInput( const QString &name, bool video, bool audio )
{
}

void EffectDesc::addParameter( EffectParamDesc *param )
{
	m_params.append(param);
}

uint EffectDesc::numParameters() const
{
	return m_params.count();
}

EffectParamDesc *EffectDesc::parameter(uint index)
{
	assert(index < numParameters());
	return m_params.at(index);
}

Effect *EffectDesc::createEffect(const QString &preset)
{
	Effect *returnEffect = 0;
	Effect *effectPreset = findPreset(preset);

	if(effectPreset) {
		returnEffect = effectPreset->clone();
	} else {
		returnEffect = new Effect(*this, this->name());
	}

	for(uint count=0; count<numParameters(); ++count) {
		returnEffect->addParameter(parameter(count)->name());
	}

	return returnEffect;
}

Effect *EffectDesc::findPreset(const QString &name)
{
	QPtrListIterator<Effect> itt(m_presets);

	while(itt.current()) {
		if(itt.current()->name() == name) return itt.current();
		++itt;
	}

	return 0;
}
