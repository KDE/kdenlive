/***************************************************************************
                          effectparamdescfactory  -  description
                             -------------------
    begin                : Sat Jan 3 2004
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
#include "effectparamdescfactory.h"

#include "effectparamdoubledesc.h"

#include <qxml.h>
#include <qstring.h>

EffectParamDescFactory::EffectParamDescFactory()
{
	m_registered.setAutoDelete(true);

	registerFactory( new EffectParamDescFactoryTemplate<EffectParamDoubleDesc>("double") );
}


EffectParamDescFactory::~EffectParamDescFactory()
{
}

EffectParamDesc *EffectParamDescFactory::createParameter(const QXmlAttributes &attributes)
{
	QString type = attributes.value( "type" );

	QPtrListIterator<EffectParamDescFactoryBase> itt(m_registered);

	while(itt.current()) {
		if(itt.current()->matchesType(type)) {
			return itt.current()->createParameter(attributes);
		}
		++itt;
	}

	return 0;
}

void EffectParamDescFactory::registerFactory(EffectParamDescFactoryBase *factory)
{
	m_registered.append(factory);
}
