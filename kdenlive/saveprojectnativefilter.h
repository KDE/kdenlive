/***************************************************************************
                          saveprojectnativefilter  -  description
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
#ifndef SAVEPROJECTNATIVEFILTER_H
#define SAVEPROJECTNATIVEFILTER_H

#include <saveprojectfilter.h>

/**
Native save format for Kdenlive. Saves everything.

@author Jason Wood
*/
class SaveProjectNativeFilter:public SaveProjectFilter {
  public:
    SaveProjectNativeFilter();

    virtual ~ SaveProjectNativeFilter();

	/** Save the document to the specified url. */
    virtual bool save(QFile & file, KdenliveDoc * document);

    virtual QStringList handledFormats() const;
};

#endif
