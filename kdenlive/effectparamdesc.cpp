/***************************************************************************
                          effectparamdesc  -  description
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
#include "effectparamdesc.h"

#include <qxml.h>

EffectParamDesc::EffectParamDesc(const QXmlAttributes & attributes)
{
    m_name = attributes.value("name");
    m_type = attributes.value("type");
    m_default = attributes.value("default").toDouble();
    m_value = m_default;
    m_description = attributes.value("description");
}

EffectParamDesc::~EffectParamDesc()
{
}


void EffectParamDesc::setValue(const double &value)
{
    m_value = value;
}

EffectKeyFrame *EffectParamDesc::createKeyFrame(double time, double value)
{
}

const QString EffectParamDesc::complexParamName(uint ix) const
{
}

const uint EffectParamDesc::complexParamNum() const
{
}

void EffectParamDesc::setDescription(const QString & description)
{
    m_description = description;
}

const QString & EffectParamDesc::description() const
{
    return m_description;
}
