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
#include <kmessagebox.h>

#include <docclipbase.h>
#include <docclipavfile.h>
#include <krendermanager.h>
#include <kaddclipcommand.h>

ClipManager::ClipManager(KRenderManager &renderManager, QWidget *parent, const char *name)
{
	m_clipList.setAutoDelete(true);
	m_temporaryClipList.setAutoDelete(true);
	m_clipCounter = 0;

	m_render = renderManager.createRenderer("Clip Manager");

	connect(m_render, SIGNAL(replyGetFileProperties(const QMap<QString, QString> &)),
  					 this, SLOT(AVFilePropertiesArrived(const QMap<QString, QString> &)));

	connect(m_render, SIGNAL(replyGetImage(const KURL &, int, const QPixmap &, int, int)),
					this, SLOT(AVImageArrived(const KURL &, int, const QPixmap &)));

	connect(m_render, SIGNAL(replyGetImage(int , const QPixmap &, int, int)),
					this, SLOT(AVImageArrived(int, const QPixmap &)));
}

ClipManager::~ClipManager()
{
}

DocClipBase *ClipManager::insertClip(const KURL &file)
{
	DocClipBase *clip = findClip(file);
	if(!clip) {
		if (!m_render->isValid(file)) {
		KMessageBox::sorry(0,i18n("The file %1 is not a valid video file for kdenlive...").arg(file.filename()));
		return 0;
		}
		
		clip = new DocClipAVFile(file.fileName(), file, m_clipCounter++);
		m_clipList.append(clip);
		m_render->getFileProperties(file);
		/* Thumbnail (none for audio files) */
		if (dynamic_cast<DocClipAVFile*>(clip)->clipType() != DocClipBase::AUDIO ) m_render->getImage(file, 1, 64, 50);
		emit clipListUpdated();
	}

	return clip;
}

DocClipBase *ClipManager::insertImageClip(const KURL &file, const QString &extension, const int &ttl, const GenTime &duration, const QString &description)
{
		DocClipBase *clip;
		clip = new DocClipAVFile(file, extension, ttl, duration, m_clipCounter);
		clip->setDescription(description);
		m_clipList.append(clip);
		m_render->getImage(file, 64, 50);
		m_clipCounter++;
		emit clipListUpdated();

	return clip;
}


DocClipBase *ClipManager::insertColorClip(const QString &color, const GenTime &duration, const QString &name, const QString &description)
{
		DocClipBase *clip;
		clip = new DocClipAVFile(color, duration, m_clipCounter);
		clip->setDescription(description);
		clip->setName(name);
		m_clipList.append(clip);
		m_render->getImage(m_clipCounter, color, 64, 50);
		m_clipCounter++;
		emit clipListUpdated();

	return clip;
}

void ClipManager::editColorClip(DocClipRef *clip, const QString &color, const GenTime &duration, const QString &name, const QString &description)
{
		clip->setDescription(description);
		clip->setCropDuration(duration);

		DocClipAVFile *avClip = dynamic_cast<DocClipAVFile*>(clip->referencedClip());
		if (avClip) {
			avClip->setColor(color);
			avClip->setName(name);
			avClip->setDuration(duration);
		}
		m_render->getImage(avClip->getId(), color, 64, 50);
		emit clipListUpdated();
}

void ClipManager::editImageClip(DocClipRef *clip, const KURL &file, const QString &extension, const int &ttl, const GenTime &duration, const QString &description)
{
		clip->setDescription(description);
		clip->setCropDuration(duration);

		DocClipAVFile *avClip = dynamic_cast<DocClipAVFile*>(clip->referencedClip());
		if (avClip) {
			avClip->setFileURL(file);
			avClip->setDuration(duration);
			m_render->getImage(file, 64, 50);
		}
		emit clipListUpdated();
}


void ClipManager::editClip(DocClipRef *clip, const KURL &file)
{
		DocClipAVFile *avClip = dynamic_cast<DocClipAVFile*>(clip->referencedClip());
		if (avClip) {
			avClip->setFileURL(file);
			m_render->getFileProperties(file);
			m_render->getImage(file, 1, 64, 50);
		emit fixClipDuration(clip->referencedClip());
		}
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

DocClipBase *ClipManager::findClipById(uint id)
{
	DocClipBase *result=NULL;

	QPtrListIterator<DocClipBase> itt(m_clipList);
	while(itt.current()) {
		if(itt.current()->toDocClipAVFile()->getId() == id) {
			result = itt.current();
		}
		++itt;
	}

	return result;
}

QDomDocument ClipManager::producersList()
{
	QDomDocument sceneList;

	QPtrListIterator<DocClipBase> itt(m_clipList);
	while(itt.current()) {
		DocClipAVFile *avClip = itt.current()->toDocClipAVFile();
		if (avClip)
		{

		if (avClip->clipType() == DocClipBase::IMAGE )
			{
			QDomElement producer = sceneList.createElement("producer");
			producer.setAttribute("id", QString("producer") + QString::number(avClip->getId()) );
			producer.setAttribute("mlt_service", "pixbuf");
			producer.setAttribute("resource", avClip->fileURL().path());
			producer.setAttribute("hide", "audio");
			sceneList.appendChild(producer);
			}

		else if (avClip->clipType() == DocClipBase::COLOR )
			{
			QDomElement producer = sceneList.createElement("producer");
			producer.setAttribute("id", QString("producer") + QString::number(avClip->getId()) );
			producer.setAttribute("mlt_service", "colour");
			producer.setAttribute("hide", "audio");
			producer.setAttribute("colour", avClip->color());
			sceneList.appendChild(producer);
			}

		else if (avClip->clipType() == DocClipBase::AUDIO )
			{
			QDomElement producer = sceneList.createElement("producer");
			producer.setAttribute("id", QString("producer") + QString::number(avClip->getId()) );
			producer.setAttribute("resource", avClip->fileURL().path());
			producer.setAttribute("hide", "video");
			sceneList.appendChild(producer);
			}
		else if (avClip->clipType() == DocClipBase::VIDEO )
			{
			QDomElement producer = sceneList.createElement("producer");
			producer.setAttribute("id", QString("producer") + QString::number(avClip->getId()) );
			producer.setAttribute("resource", avClip->fileURL().path());
			producer.setAttribute("hide", "audio");
			sceneList.appendChild(producer);
			}

		else 
			{
			QDomElement producer = sceneList.createElement("producer");
			producer.setAttribute("id", QString("producer") + QString::number(avClip->getId()) );
			producer.setAttribute("resource", avClip->fileURL().path());
			sceneList.appendChild(producer);
			}

		/*

		if (avClip->clipType() == DocClipBase::AV )
			{
			QDomElement producer = sceneList.createElement("producer");
			producer.setAttribute("id", QString("video_producer") + QString::number(avClip->getId()) );
			producer.setAttribute("audio_index", QString::number(-1));
			producer.setAttribute("resource", avClip->fileURL().path());
			sceneList.appendChild(producer);

			QDomElement producer2 = sceneList.createElement("producer");
			producer2.setAttribute("id", QString("audio_producer") + QString::number(avClip->getId()) );
			producer2.setAttribute("video_index", QString::number(-1));
			producer2.setAttribute("resource", avClip->fileURL().path());
			sceneList.appendChild(producer2);
			}*/
		}
		++itt;
	}
	return sceneList;
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

void ClipManager::AVImageArrived( int id, const QPixmap &pixmap)
{
	DocClipBase *clip = findClipById(id);
	if(clip) {
		clip->setThumbnail(pixmap);
	}
}

void ClipManager::AVImageArrived( const KURL &url, int frame, const QPixmap &pixmap)
{
	DocClipBase *clip = findClip(url);

	if(clip) {
		clip->setThumbnail(pixmap);
	}
}

