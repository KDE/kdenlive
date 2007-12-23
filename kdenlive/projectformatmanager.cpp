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
#include "projectformatmanager.h"

#include <qfile.h>

#include <kdebug.h>
#include <kio/job.h>
#include <kio/netaccess.h>
#include <kmimetype.h>
#include <ktempfile.h>
#include <kurl.h>
#include <klocale.h>
#include <kmessagebox.h>

#include "kdenlivedoc.h"

#include "loadprojectkinofilter.h"
#include "loadprojectnativefilter.h"
#include "saveprojectnativefilter.h"
#include "saveprojectscenelistfilter.h"

ProjectFormatManager::ProjectFormatManager()
{
    m_saveFilters.setAutoDelete(true);
    m_loadFilters.setAutoDelete(true);

    registerLoadFilter(new LoadProjectNativeFilter());
    registerLoadFilter(new LoadProjectKinoFilter());

    registerSaveFilter(new SaveProjectNativeFilter());
    registerSaveFilter(new SaveProjectScenelistFilter());
}


ProjectFormatManager::~ProjectFormatManager()
{
}

bool ProjectFormatManager::mergeDocument(const KURL & url,
    KdenliveDoc * document)
{
    if (url.isEmpty())
	return false;

    KMimeType::Ptr format = KMimeType::findByURL(url);

    LoadProjectFilter *filter = findLoadFormat(format->name());
    if (filter) {
	QString tmpfile;
	if (KIO::NetAccess::download(url, tmpfile, 0)) {
	    QFile file(tmpfile);
	    if (file.open(IO_ReadOnly)) {
		filter->merge(file, document);
	    }
	    KIO::NetAccess::removeTempFile(tmpfile);
	    //document->initSceneListGeneration();
	    QTimer::singleShot(200, document, SLOT(initSceneListGeneration()));
	    return true;
	}
    }

    return false;
}

bool ProjectFormatManager::openDocument(const KURL & url,
    KdenliveDoc * document)
{
    if (url.isEmpty())
	return false;

    KMimeType::Ptr format = KMimeType::findByURL(url);

    LoadProjectFilter *filter = findLoadFormat(format->name());
    // if(url.filename().right(9) == ".kdenlive")
    if (filter) {
	QString tmpfile;
	if (KIO::NetAccess::download(url, tmpfile, 0)) {
	    QFile file(tmpfile);
	    if (file.open(IO_ReadOnly)) {
                document->closeDocument();
		filter->load(file, document);
		document->setURL(url);
	    }
	    KIO::NetAccess::removeTempFile(tmpfile);
	    //document->initSceneListGeneration();
	    QTimer::singleShot(500, document, SLOT(initSceneListGeneration()));
	    return true;
	}
//      } else {
//              document->clipManager().insertClip(url);
    }

    return false;
}

bool ProjectFormatManager::saveDocument(const KURL & url,
    KdenliveDoc * document)
{
    if (url.isEmpty())
	return false;
    bool result = false;
    KMimeType::Ptr format = KMimeType::findByURL(url);
    SaveProjectFilter *filter = findSaveFormat(format->name());

    if (filter) {
	KTempFile file;
	//file.setAutoDelete(true);

	if ((filter->save(*file.file(), document))) {
	    file.close();
	    if (!KIO::NetAccess::upload(file.name(), url, 0)) {
		//kdError() << "Could not upload file to correct location" << endl;
		KMessageBox::sorry(0, i18n("Could not save file %1.\nPlease check your permissions").arg(url.path()));
	    }
	    else result = true;
	} else {
	    KMessageBox::sorry(0, i18n("Could not save file %1.\nPlease check your permissions").arg(file.name()));
	    //kdError() << "Save failed" << endl;
	}
    }

    if (result) document->setModified(false);
    return result;
}

bool ProjectFormatManager::backupDocument(const KURL & url,
    KdenliveDoc * document)
{
    if (url.isEmpty())
	return false;
    bool result = false;
    KMimeType::Ptr format = KMimeType::findByURL(url);
    SaveProjectFilter *filter = m_saveFilters.first();

    if (filter) {
	KTempFile file;
	//file.setAutoDelete(true);

	if ((filter->save(*file.file(), document, false))) {
	    file.close();
	    if (!KIO::NetAccess::upload(file.name(), url, 0)) {
		//kdError() << "Could not upload file to correct location" << endl;
		KMessageBox::sorry(0, i18n("Could not save file %1.\nPlease check your permissions").arg(url.path()));
	    }
	    else result = true;
	} else {
	    kdError() << "Save failed" << endl;
	}
    }
    return result;
}

void ProjectFormatManager::registerSaveFilter(SaveProjectFilter * filter)
{
    m_saveFilters.append(filter);
}

void ProjectFormatManager::registerLoadFilter(LoadProjectFilter * filter)
{
    m_loadFilters.append(filter);
}

LoadProjectFilter *ProjectFormatManager::
findLoadFormat(const QString & format)
{
    QPtrListIterator < LoadProjectFilter > itt(m_loadFilters);
    while (itt.current()) {
	if (itt.current()->handlesFormat(format)) {
	    return itt.current();
	}
	++itt;
    }

    return m_loadFilters.first();
}

SaveProjectFilter *ProjectFormatManager::
findSaveFormat(const QString & format)
{
    QPtrListIterator < SaveProjectFilter > itt(m_saveFilters);

    while (itt.current()) {
	if (itt.current()->handlesFormat(format)) {
	    return itt.current();
	}
	++itt;
    }

    return m_saveFilters.first();
}

/** Returns the mime types that can be loaded in */
QString ProjectFormatManager::loadMimeTypes()
{
    QPtrListIterator < LoadProjectFilter > itt(m_loadFilters);

    QStringList list;

    while (itt.current()) {
	QStringList extraList = itt.current()->handledFormats();

	for (QStringList::Iterator extraItt = extraList.begin();
	    extraItt != extraList.end(); ++extraItt) {
	    list.append(*extraItt);
	}

	++itt;
    }

    return list.join(" ");
}

/** Returns the mime types that can be saved out */
QString ProjectFormatManager::saveMimeTypes()
{
    QPtrListIterator < SaveProjectFilter > itt(m_saveFilters);

    QStringList list;

    while (itt.current()) {
	QStringList extraList = itt.current()->handledFormats();

	for (QStringList::Iterator extraItt = extraList.begin();
	    extraItt != extraList.end(); ++extraItt) {
	    list.append(*extraItt);
	}

	++itt;
    }

    return list.join(" ");
}
