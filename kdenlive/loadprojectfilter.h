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
#ifndef LOADPROJECTFILTER_H
#define LOADPROJECTFILTER_H

#include <qstringlist.h>

class QFile;
class KdenliveDoc;

/**
Base class for loading projects into kdenlive

@author Jason Wood
*/
class LoadProjectFilter{
public:
	LoadProjectFilter();

	virtual ~LoadProjectFilter();

	/** Returns a list of formats handled by this filter. */
	virtual QStringList handledFormats() const = 0;
	
	/** Returns true if this filter handles the format passed */
	virtual bool handlesFormat(const QString &format);
	
	/** load the specified url into the document passed. The document is empty when 
	 * passed in. Returns true on success.
	 **/
	virtual bool load(QFile &file, KdenliveDoc *document) = 0;
};

#endif
