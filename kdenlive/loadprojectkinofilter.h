/***************************************************************************
                          loadprojectkinofilter  -  description
                             -------------------
    begin                : Sun Dec 14 2003
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
#ifndef LOADPROJECTKINOFILTER_H
#define LOADPROJECTKINOFILTER_H

#include <loadprojectfilter.h>

#include "gentime.h"

class QDomElement;
class DocTrackBase;
class KdenliveDoc;

/**
Loads a kino project into kdenlive.

@author Jason Wood
*/
class LoadProjectKinoFilter:public LoadProjectFilter {
  public:
    LoadProjectKinoFilter();

    ~LoadProjectKinoFilter();

    virtual bool load(QFile & file, KdenliveDoc * document);
    virtual bool merge(QFile & file, KdenliveDoc * document, bool insertTimeLine = false, GenTime insertTime = GenTime());
    virtual QStringList handledFormats() const;
  private:
     GenTime loadSeq(const GenTime & currentTrackTime,
	const QDomElement & seqElem, DocTrackBase * track,
	KdenliveDoc * document);
};

#endif
