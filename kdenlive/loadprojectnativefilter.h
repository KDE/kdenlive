/***************************************************************************
                          loadprojectnativefilter  -  description
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
#ifndef LOADPROJECTNATIVEFILTER_H
#define LOADPROJECTNATIVEFILTER_H

#include <loadprojectfilter.h>

class QDomElement;

/**
Loads in native Kdenlive format

@author Jason Wood
*/
class LoadProjectNativeFilter:public LoadProjectFilter {
  public:
    LoadProjectNativeFilter();

    virtual ~ LoadProjectNativeFilter();

    QStringList handledFormats() const;
    virtual bool load(QFile & file, KdenliveDoc * document);
    virtual bool merge(QFile & file, KdenliveDoc * document, bool insertTimeLine = false, GenTime insertTime = GenTime());
  private:
    void loadAVFileList(QDomElement & element, KdenliveDoc * document);
    void loadTrackList(QDomElement & element, KdenliveDoc * document, GenTime insertTime = GenTime());
    void addToDocument(const QString & parent, QDomElement & clip,
	KdenliveDoc * document);
};

#endif
