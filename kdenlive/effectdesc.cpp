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

EffectDesc::EffectDesc(const QString & name, const QString stringId, const QString & tag, const QString & description, const QString & author, EFFECTTYPE type, bool mono):
m_name(name), m_tag(tag), m_type(type), m_id(stringId), m_mono(mono), m_description(description), m_author(author)
{
    m_params.setAutoDelete(true);

    // TODO - what happens with the m_effect list? Does it get populated and then never deleted?
}

EffectDesc::~EffectDesc()
{
}

/** Returns the name of this effect. */
const QString & EffectDesc::name() const
{
    return m_name;
}

/** Returns the tag for this effect. */
const QString & EffectDesc::tag() const
{
    return m_tag;
}

/** Returns the type for this effect (audio or video). */
EFFECTTYPE EffectDesc::type() const
{
    return m_type;
}

/** Returns the type for this effect (audio or video). */
const QString & EffectDesc::stringId() const
{
   return m_id;
}

/** Is this effect an auido mono effect. */
bool EffectDesc::isMono() const
{
    return m_mono;
}

/** Returns effect full description. */
const QString & EffectDesc::description() const
{
    return m_description;
}

/** Returns effect author name. */
const QString & EffectDesc::author() const
{
    return m_author;
}

/** Adds an input to this description. An input might be a video stream, and audio stream, or it may require both. */
void EffectDesc::addInput(const QString & name, bool video, bool audio)
{
}

void EffectDesc::addParameter(EffectParamDesc * param)
{
    m_params.resize(m_params.size() + 1);
    m_params.insert(m_params.size() - 1, param);
}

uint EffectDesc::numParameters() const
{
    return m_params.count();
}

EffectParamDesc *EffectDesc::parameter(uint index) const
{
    assert(index < numParameters());
    return m_params.at(index);
}

Effect *EffectDesc::createEffect()
{
    Effect *returnEffect = new Effect(*this, this->stringId());

    for (uint count = 0; count < numParameters(); ++count) {
	returnEffect->addParameter(parameter(count)->name());
    }
    returnEffect->addInitialKeyFrames(0);
    return returnEffect;
}
