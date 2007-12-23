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
#include "saveprojectscenelistfilter.h"

#include <qfile.h>
#include <qdom.h>
#include <qstring.h>

#include <kdenlivedoc.h>
#include <docclipproject.h>

SaveProjectScenelistFilter::SaveProjectScenelistFilter()
:  SaveProjectFilter()
{
}


SaveProjectScenelistFilter::~SaveProjectScenelistFilter()
{
}


bool SaveProjectScenelistFilter::save(QFile & file, KdenliveDoc * document, bool)
{
    QDomDocument doc;
 
    // include a copy of the MLT playlist so that the project file can be played directly
    QDomDocument westleyList = document->projectClip().generateSceneList(true);
    QDomNode playlist = doc.importNode(westleyList.documentElement(), true);
    doc.appendChild(playlist);
    
    QCString save = doc.toString().utf8();
    if (file.writeBlock(save, save.length()) == -1) return false;
    return true;
}

QStringList SaveProjectScenelistFilter::handledFormats() const
{
    QStringList list;
    list.append("application/vnd.westley.scenelist");
    return list;
}
