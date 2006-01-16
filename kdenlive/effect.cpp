/***************************************************************************
                          effect.cpp  -  description
                             -------------------
    begin                : Sun Feb 16 2003
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

#include "effect.h"

#include "effectdesc.h"
#include "effectparameter.h"
#include "effectparamdescfactory.h"
#include "effectdoublekeyframe.h"
#include "effectkeyframe.h"
#include "effectparamdesc.h"

#include <kdebug.h>
#include <qdom.h>


Effect::Effect(const EffectDesc & desc, const QString & name):m_desc(desc),
m_name(name)
{
}

Effect::~Effect()
{
}

QDomDocument Effect::toXML()
{
    QDomDocument doc;

    QDomElement effect = doc.createElement("effect");

    effect.setAttribute("name", name());
    effect.setAttribute("type", m_desc.name());

    for (uint i = 0; i < m_desc.numParameters(); i++) {
	EffectParamDesc *paramdesc = m_desc.parameter(i);
	QDomElement param = doc.createElement("param");
	param.setAttribute("name", paramdesc->name());
	param.setAttribute("type", paramdesc->type());
	param.setAttribute("value", paramdesc->value());
	param.setAttribute("default", paramdesc->defaultValue());
	param.setAttribute("max", paramdesc->max());
	param.setAttribute("min", paramdesc->min());
	if (paramdesc->type() == "double")
	    for (uint j = 0; j < m_paramList.at(i)->numKeyFrames(); j++) {
		QDomElement keyframe = doc.createElement("keyframe");
		keyframe.setAttribute("time",
		    m_paramList.at(i)->keyframe(j)->time());
		keyframe.setAttribute("value",
		    m_paramList.at(i)->keyframe(j)->toDoubleKeyFrame()->
		    value());
		param.appendChild(keyframe);
	    }
	effect.appendChild(param);
    }

    doc.appendChild(effect);
    return doc;
}

uint Effect::addKeyFrame(const uint ix, double time)
{
    return m_paramList.at(ix)->addKeyFrame(effectDescription().
	parameter(ix)->createKeyFrame(time));
}

void Effect::addKeyFrame(const uint ix, double time, double value)
{
    m_paramList.at(ix)->addKeyFrame(effectDescription().parameter(ix)->
	createKeyFrame(time, value));
}

void Effect::addParameter(const QString & name)
{
    m_paramList.append(new EffectParameter(name));
}

EffectParameter *Effect::parameter(const uint ix)
{
    return m_paramList.at(ix);
}

Effect *Effect::clone()
{
    return Effect::createEffect(m_desc, toXML().documentElement());
}

// static
Effect *Effect::createEffect(const EffectDesc & desc,
    const QDomElement & effect)
{
    Effect *result = 0;

    if (effect.tagName() == "effect") {
	QString name = effect.attribute("name", "");
	QString type = effect.attribute("type", "");

	if (type != desc.name()) {
	    kdError() <<
		"Effect::createEffect() failed integrity check - desc.name() == "
		<< desc.name() << ", type == " << type << endl;
	}
	result = new Effect(desc, name);

	QDomNode node = effect.firstChild();
	uint index = 0;
	EffectParamDescFactory m_effectDescParamFactory;

	while (!node.isNull()) {
	    QDomElement e = node.toElement();
	    if (!e.isNull()) {
		if (e.tagName() == "param") {
		    result->addParameter(e.attribute("name", ""));
		    QDomNode keyNode = e.firstChild();
		    while (!keyNode.isNull()) {
			QDomElement k = keyNode.toElement();
			if (k.tagName() == "keyframe") {
			    result->addKeyFrame(index, k.attribute("time",
				    "").toDouble(), k.attribute("value",
				    "").toDouble());
			}
			keyNode = keyNode.nextSibling();
		    }
		    index++;
		}
	    }
	    node = node.nextSibling();
	}
    } else {
	kdError() <<
	    "Trying to create an effect from xml tag that is not <effect>"
	    << endl;
    }
    return result;
}
