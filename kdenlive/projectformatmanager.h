/***************************************************************************
                          projectformatmanager  -  description
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
#ifndef PROJECTFORMATMANAGER_H
#define PROJECTFORMATMANAGER_H

#include <qptrlist.h>
#include <qstring.h>

#include "loadprojectfilter.h"
#include "saveprojectfilter.h"

class QString;
class KURL;
class KdenliveDoc;

/**
Manages the various load/save formats that Kdenlive is capable of reading, and provides interfaces to load/save projects.

@author Jason Wood
*/
class ProjectFormatManager {
  public:
    ProjectFormatManager();

    ~ProjectFormatManager();

	/** loads the document by filename and format and emits the updateViews() signal */
    bool openDocument(const KURL & url, KdenliveDoc * document);
	/** merge the document with currently open document */
    bool mergeDocument(const KURL & url, KdenliveDoc * document);
	/** saves the document under filename and format.*/
    bool saveDocument(const KURL & url, KdenliveDoc * document);
	/** saves a copy of the document for crash recovery.*/
    bool backupDocument(const KURL & url, KdenliveDoc * document);
	/** Returns the mime types that can be loaded in */
    QString loadMimeTypes();

	/** Returns the mime types that can be saved out */
    QString saveMimeTypes();
  private:
     QPtrList < SaveProjectFilter > m_saveFilters;
     QPtrList < LoadProjectFilter > m_loadFilters;

	/** Register a save filter with the project managaer. */
    void registerSaveFilter(SaveProjectFilter * filter);
	/** Register a load filter with the project managaer. */
    void registerLoadFilter(LoadProjectFilter * filter);

	/** Find a project filter that can load files of the given foramt. */
    LoadProjectFilter *findLoadFormat(const QString & format);
	/** Find a project filter that can save files of the given foramt. */
    SaveProjectFilter *findSaveFormat(const QString & format);

};

#endif
