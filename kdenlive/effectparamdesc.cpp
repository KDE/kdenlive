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
#include <klocale.h>

#include "effectparamdesc.h"
#include "effectparamdescfactory.h"

EffectParamDesc::EffectParamDesc(const QDomElement & parameter)
{
    m_xml = parameter;
    m_name = parameter.attribute("name", QString::null);
    m_type = parameter.attribute("type", QString::null);
    m_default = parameter.attribute("default", QString::null);
    m_factor = parameter.attribute("factor", QString::null).toDouble();
    if (m_factor == 0.0) m_factor = 1.0;
    m_value = m_default;
    QDomNode namenode = parameter.elementsByTagName("name").item(0);
    m_description = i18n(namenode.toElement().text());
}

EffectParamDesc::~EffectParamDesc()
{
}

EffectParamDesc *EffectParamDesc::clone()
{
    EffectParamDescFactory effectDescParamFactory;
    return effectDescParamFactory.createParameter(m_xml);
}

const bool EffectParamDesc::isComplex() const
{
}

void EffectParamDesc::setValue(const QString &value)
{
    m_value = value;
}

EffectKeyFrame *EffectParamDesc::createKeyFrame(double time, double value)
{
}

EffectKeyFrame *EffectParamDesc::createKeyFrame(double time, QStringList parametersList)
{
}

const QMap <double, QString> EffectParamDesc::initialKeyFrames() const
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
