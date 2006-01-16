/***************************************************************************
                          loadprojectfilter  -  description
                             -------------------
    begin                : Wed Dec 3 2003
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
#include "loadprojectfilter.h"

LoadProjectFilter::LoadProjectFilter()
{
}


LoadProjectFilter::~LoadProjectFilter()
{
}

// virtual 
bool LoadProjectFilter::handlesFormat(const QString & format)
{
    QStringList formats = handledFormats();

    for (QStringList::Iterator itt = formats.begin(); itt != formats.end();
	++itt) {
	if (*itt == format)
	    return true;
    }

    return false;
}
