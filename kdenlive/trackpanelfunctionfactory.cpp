/***************************************************************************
                          trackpanelfunctionfactory  -  description
                             -------------------
    begin                : Sun Dec 28 2003
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
#include "trackpanelfunctionfactory.h"

#include "trackpanelfunction.h"

#include "kdebug.h"

TrackPanelFunctionFactory::TrackPanelFunctionFactory()
{
}


TrackPanelFunctionFactory::~TrackPanelFunctionFactory()
{
	QMap<QString,  TrackPanelFunction*>::iterator itt = m_functionMap.begin();

	while(itt != m_functionMap.end()) {
		delete (itt.data());
		itt.data() = 0;
		++itt;
	}
}

void TrackPanelFunctionFactory::registerFunction(const QString &name, TrackPanelFunction *function)
{
	if(!m_functionMap.contains(name)) {
		m_functionMap[name] = function;
	} else {
		kdError() << "Factory already contains a function called " << name << endl;
	}
}

TrackPanelFunction *TrackPanelFunctionFactory::function(const QString &name)
{
	if(m_functionMap.contains(name)) {
		return m_functionMap[name];
	} else {
		kdError() << "No function called " << name << " found in factory" << endl;
	}

	return 0;
}
