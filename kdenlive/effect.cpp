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

#include <kdebug.h>
#include <qdom.h>

Effect::Effect(const EffectDesc &desc, const QString &name) :
				m_desc(desc),
				m_name(name)
{
}

Effect::~Effect()
{
}

QDomDocument Effect::toXML() const
{
	QDomDocument doc;

	QDomElement effect = doc.createElement("effect");

	effect.setAttribute("name", name());
	effect.setAttribute("type", m_desc.name());

	doc.appendChild(effect);

	return doc;
}

void Effect::addParameter(const QString &name)
{
	m_paramList.append(new EffectParameter(name));
}

Effect *Effect::clone() const
{
	return Effect::createEffect(m_desc, toXML().documentElement());
}

// static
Effect *Effect::createEffect(const EffectDesc &desc, const QDomElement &effect)
{
	Effect *result = 0;
	if(effect.tagName() == "effect") {
		QString name = effect.attribute("name", "");
		QString type= effect.attribute("type", "");

		if(type != desc.name()) {
			kdError() << "Effect::createEffect() failed integrity check - desc.name() == " << desc.name() << ", type == " << type << endl;
		}

		result = new Effect(desc, name);
	} else {
		kdError() << "Trying to create an effect from xml tag that is not <effect>" << endl;
	}
	return result;
}

