/***************************************************************************
                          clipmanager.cpp  -  description
                             -------------------
    begin                : Wed Sep 17 08:36:16 GMT 2003
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

#include "clipmanager.h"

// include files for KDE
#include <klocale.h>
#include <kdebug.h>

#include <docclipbase.h>
#include <docclipavfile.h>
#include <krendermanager.h>
#include <kaddclipcommand.h>

ClipManager::ClipManager(KRenderManager &renderManager, QWidget *parent, const char *name)
{
	m_clipList.setAutoDelete(true);
	m_temporaryClipList.setAutoDelete(true);

	m_render = renderManager.createRenderer(i18n("Clip Manager"));

	connect(m_render, SIGNAL(replyGetFileProperties(const QMap<QString, QString> &)),
  					 this, SLOT(AVFilePropertiesArrived(const QMap<QString, QString> &)));
	connect(m_render, SIGNAL(replyGetImage(const KURL &, int, const QPixmap &, int, int)),
					this, SLOT(AVImageArrived(const KURL &, int, const QPixmap &)));
}

ClipManager::~ClipManager()
{
}

DocClipBase *ClipManager::insertClip(const KURL &file)
{
	DocClipBase *clip = findClip(file);

	if(!clip) {
		clip = new DocClipAVFile(file.fileName(), file);
		m_clipList.append(clip);
		m_render->getFileProperties(file);
		m_render->getImage(file, 1, 64, 64);
		emit clipListUpdated();
	}

	return clip;
}

DocClipBase *ClipManager::findClip(const QDomElement &clip)
{
	DocClipBase *result=NULL;

	QPtrListIterator<DocClipBase> itt(m_clipList);
	while(itt.current()) {
		if(itt.current()->matchesXML(clip)) {
			result = itt.current();
		}
		++itt;
	}

	return result;
}

DocClipBase *ClipManager::findClip(const KURL &file)
{
	DocClipBase *result=0;

	QPtrListIterator<DocClipBase> itt(m_clipList);
	while(itt.current()) {
		DocClipAVFile *avClip = itt.current()->toDocClipAVFile();

		if(avClip && (avClip->fileURL() == file)) {
			result = avClip;
			break;
		}

		++itt;
	}

	return result;
}

DocClipBase *ClipManager::insertClip(const QDomElement &clip)
{
	DocClipBase *result = findClip(clip);

	if(!result) {
		result = DocClipBase::createClip(m_render->effectList(), *this, clip);
		if(result) {
			m_clipList.append(result);
		} else {
			kdError() << "Could not insert clip into clip manager - createClip failed" << endl;
		}
	}

	return result;
}

void ClipManager::clear()
{
#warning "This might blow up spectacularly - this implementation does not check"
#warning "and clean up any references to said clips."
	m_clipList.clear();
	m_temporaryClipList.clear();
}

DocClipAVFile *ClipManager::findAVFile(const KURL &url)
{
	DocClipAVFile *file = NULL;

	QPtrListIterator<DocClipBase> itt(m_clipList);

	while(itt.current())
	{
		DocClipBase *clip = itt.current();
		if(clip->isDocClipAVFile()) {
			DocClipAVFile *avClip = clip->toDocClipAVFile();

			if(avClip->fileURL() == url) {
				file = avClip;
				break;
			}
		}
		++itt;
	}

	return file;
}

void ClipManager::AVFilePropertiesArrived(const QMap<QString, QString> &properties)
{
	if(!properties.contains("filename")) {
		kdError() << "File properties returned with no file name attached" << endl;
		return;
	}

	DocClipAVFile *file = findAVFile(KURL(properties["filename"]));
	if(!file) {
		kdWarning() << "File properties returned for a non-existant AVFile" << endl;
		return;
	}

	file->calculateFileProperties(properties);

	emit clipChanged(file);
}


void ClipManager::generateFromXML(KRender *render, const QDomElement &e)
{
	m_clipList.generateFromXML(render->effectList(), *this, render, e);
}

QDomDocument ClipManager::toXML(const QString &element)
{
	return m_clipList.toXML(element);
}

void ClipManager::removeClip(const KURL &file)
{
#warning - to be written
}

void ClipManager::removeClip(const QDomElement &clip)
{
#warning - to be written
}

/** Adds a temporary clip. This is a clip that does not "exist" in the project, but of which
 * some stored information is required. */
DocClipBase *ClipManager::addTemporaryClip(const QDomElement &clip)
{
#warning - to be written.
	return insertClip(clip);
}

DocClipBase *ClipManager::addTemporaryClip(const KURL &file)
{
#warning - to be written.
	return insertClip(file);
}

void ClipManager::AVImageArrived( const KURL &url, int frame, const QPixmap &pixmap)
{
	DocClipBase *clip = findClip(url);

	if(clip) {
		clip->setThumbnail(pixmap);
	}
}

