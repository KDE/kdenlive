/***************************************************************************
                          docclipproject.cpp  -  description
                             -------------------
    begin                : Thu Jun 20 2002
    copyright            : (C) 2002 by Jason Wood
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

#include "docclipproject.h"

DocClipProject::DocClipProject() :
  			DocClipBase()
{
}

DocClipProject::~DocClipProject()
{
}

GenTime DocClipProject::duration() {
	return GenTime(0.0);
}

/** No descriptions */
KURL DocClipProject::fileURL()
{
	KURL temp;

	return temp;
}

QDomDocument DocClipProject::toXML() 
{
	QDomDocument doc = DocClipBase::toXML();
	return doc;
}

