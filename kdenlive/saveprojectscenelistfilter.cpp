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

#include <qdom.h>
#include <qstring.h>

#include <kdenlivedoc.h>
#include <docclipproject.h>

SaveProjectScenelistFilter::SaveProjectScenelistFilter()
 : SaveProjectFilter()
{
}


SaveProjectScenelistFilter::~SaveProjectScenelistFilter()
{
}


bool SaveProjectScenelistFilter::save(QFile& file, KdenliveDoc* document)
{
	QDomDocument doc = document->projectClip().generateSceneList();
	QString save = doc.toString();
	file.writeBlock(save, save.length());
}

QStringList SaveProjectScenelistFilter::handledFormats() const
{
	QStringList list;
	list.append("application/vnd.kde.kdenlive.scenelist");
	return list;
}

