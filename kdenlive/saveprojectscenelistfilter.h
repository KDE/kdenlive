/***************************************************************************
                          saveprojectscenelistfilter  -  description
                             -------------------
    begin                : Sat Dec 20 2003
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
#ifndef SAVEPROJECTSCENELISTFILTER_H
#define SAVEPROJECTSCENELISTFILTER_H

#include <saveprojectfilter.h>

/**
Saves the document as a scenelist file.

@author Jason Wood
*/
class SaveProjectScenelistFilter:public SaveProjectFilter {
  public:
    SaveProjectScenelistFilter();

    ~SaveProjectScenelistFilter();

    virtual bool save(QFile & file, KdenliveDoc * document);
    virtual QStringList handledFormats() const;

};

#endif
