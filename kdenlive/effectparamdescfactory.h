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
#ifndef EFFECTPARAMDESCFACTORY_H
#define EFFECTPARAMDESCFACTORY_H

#include <qstring.h>
#include <qptrlist.h>

class EffectParamDesc;
class QXmlAttributes;

class EffectParamDescFactoryBase {
public:
	EffectParamDescFactoryBase(const QString &type) : m_type(type) {;}
	virtual ~EffectParamDescFactoryBase() {;}

	bool matchesType(const QString &type) const
	{
		return m_type == type;
	}

	virtual EffectParamDesc *createParameter(const QXmlAttributes &attributes) = 0;
private:
	QString m_type;
};

template<class Desc> class EffectParamDescFactoryTemplate : public EffectParamDescFactoryBase
{
public:
	EffectParamDescFactoryTemplate(const QString &type) :
				EffectParamDescFactoryBase(type)
	{;}

	virtual EffectParamDesc *createParameter(const QXmlAttributes &attributes) {
		return new Desc(attributes);
	}
};

/**
A factory for creating EffectParamDesc objects.

@author Jason Wood
*/
class EffectParamDescFactory{
public:
    EffectParamDescFactory();

    ~EffectParamDescFactory();

	/** Construct an EffectParamDesc from the attribute list passed. Exactly what attributes are in the list will depend on the type of parameter
	created - the only attribute that is guaranteed to exist is "type". */
	EffectParamDesc *createParameter(const QXmlAttributes &attributes);

	void registerFactory(EffectParamDescFactoryBase *factory);
private:
	QPtrList<EffectParamDescFactoryBase> m_registered;
};

#endif
