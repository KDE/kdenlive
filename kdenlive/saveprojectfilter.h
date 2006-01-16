/***************************************************************************
                          saveprojectfilter  -  description
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
#ifndef SAVEPROJECTFILTER_H
#define SAVEPROJECTFILTER_H

#include <qstringlist.h>

class KdenliveDoc;
class QFile;

/**
Baseclass for saving projects.

@author Jason Wood
*/
class SaveProjectFilter {
  public:
    SaveProjectFilter();

    virtual ~ SaveProjectFilter();

	/** Save the document to the specified url. */
    virtual bool save(QFile & file, KdenliveDoc * document) = 0;

	/** Returns a list of formats handled by this filter. */
    virtual QStringList handledFormats() const = 0;

	/** Returns true if this filter handles the format passed */
    virtual bool handlesFormat(const QString & format);
};

#endif
