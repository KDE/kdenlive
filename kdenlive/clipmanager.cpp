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


#include <qfile.h>

// include files for KDE
#include <klocale.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kio/netaccess.h>

#include <docclipbase.h>
#include <docclipavfile.h>
#include <doccliptextfile.h>
#include <krendermanager.h>
#include <kaddclipcommand.h>
#include <titlewidget.h>


ClipManager::ClipManager(KRender *render, QWidget * parent, const char *name) //Manager & renderManager
{
    m_clipList.setAutoDelete(true);
    m_temporaryClipList.setAutoDelete(true);
    m_clipCounter = 0;

    m_render = render; //renderManager.createRenderer("Clip Manager");

    connect(m_render, SIGNAL(replyGetFileProperties(const QMap < QString,
		QString > &)), this,
	SLOT(AVFilePropertiesArrived(const QMap < QString, QString > &)));

    connect(m_render, SIGNAL(replyGetImage(const KURL &, int,
		const QPixmap &, int, int)), this,
	SLOT(AVImageArrived(const KURL &, int, const QPixmap &)));

    connect(m_render, SIGNAL(replyGetImage(int, const QPixmap &, int,
		int)), this, SLOT(AVImageArrived(int, const QPixmap &)));
    
    connect(this, SIGNAL(getFileProperties(KURL)), m_render, SLOT(getFileProperties(KURL)));
}

ClipManager::~ClipManager()
{
}

DocClipBase *ClipManager::insertClip(const KURL & file, int clipId)
{
    if (!KIO::NetAccess::exists(file, true, 0)) {
	if (KMessageBox::questionYesNo(0, i18n("Cannot open file %1\nDo you want to search for the file or remove it from the project ?").arg(file.path()), i18n("Missing File"), i18n("Find File"), i18n("Remove")) == KMessageBox::Yes) {
	KURL url = KFileDialog::getOpenURL(file.path());
	if (!url.isEmpty()) return insertClip(url, clipId);
	else return 0;
	}
	else return 0;
    }
    DocClipBase *clip = findClip(file);
    if (!clip) {
	if (!m_render->isValid(file)) {
	    KMessageBox::sorry(0,
		i18n
		("The file %1 is not a valid video file for kdenlive...").
		arg(file.filename()));
	    return 0;
	}

        if (clipId == -1) clip = new DocClipAVFile(file.fileName(), file, m_clipCounter++);
        else {
            clip = new DocClipAVFile(file.fileName(), file, clipId);
            if (clipId>=m_clipCounter) m_clipCounter = clipId+1;
        }
	m_clipList.append(clip);
        emit getFileProperties(file);
	emit clipListUpdated();
	return clip;
    }
    return 0;
}

DocClipBase *ClipManager::insertImageClip(const KURL & file,
    const QString & extension, const int &ttl, const GenTime & duration,
    const QString & description, bool alphaTransparency, int clipId)
{
    if (!KIO::NetAccess::exists(file, true, 0) && !file.filename().startsWith(".all.")) {
	if (KMessageBox::questionYesNo(0, i18n("Cannot open file %1\nDo you want to search for the file or remove it from the project ?").arg(file.path()), i18n("Missing File"), i18n("Find File"), i18n("Remove")) == KMessageBox::Yes) {
	KURL url = KFileDialog::getOpenURL(file.path());
	if (!url.isEmpty()) return insertImageClip(url, extension, ttl, duration, description, alphaTransparency, clipId);
	else return 0;
	}
	else return 0;
    }
    DocClipBase *clip = findClip(file);
    if (!clip) {
	if (!m_render->isValid(file)) {
	    KMessageBox::sorry(0,
		i18n
		("The file %1 is not a valid video file for kdenlive...").
		arg(file.filename()));
	    return 0;
	}

    	if (clipId == -1) clip = new DocClipAVFile(file, extension, ttl, duration, alphaTransparency, m_clipCounter++);
    	else {
        	clip = new DocClipAVFile(file, extension, ttl, duration, alphaTransparency, clipId);
        	if (clipId>=m_clipCounter) m_clipCounter = clipId+1;
    	}
	if (ttl != 0) {
		int imageCount = duration.frames(KdenliveSettings::defaultfps()) / ttl;
		clip->setName(clip->name() + i18n(" [%1 images]").arg(QString::number(imageCount)));
	}
    	clip->setDescription(description);
    	m_clipList.append(clip);

    	m_render->getImage(file, 50, 40);

    	emit clipListUpdated();
	return clip;
    }
    return 0;
}

DocClipBase *ClipManager::insertColorClip(const QString & color,
                                          const GenTime & duration, const QString & name,
                                          const QString & description, int clipId)
{
    DocClipBase *clip;
    if (clipId == -1) clip = new DocClipAVFile(color, duration, m_clipCounter);
    else {
        clip = new DocClipAVFile(color, duration, clipId);
        m_clipCounter = clipId;
    }
    clip->setDescription(description);
    clip->setName(name);
    m_clipList.append(clip);
    m_render->getImage(m_clipCounter, color, 50, 40);
    m_clipCounter++;
    emit clipListUpdated();
    return clip;
}

DocClipBase *ClipManager::insertTextClip(
    const GenTime & duration, const QString & name,
    const QString & description, const QDomDocument &xml, const KURL url, QPixmap &pix, bool alphaTransparency, int clipId)
{
    if (!QFile(url.path()).exists() || pix.isNull()) {
        titleWidget *txtWidget=new titleWidget(10,10);
        txtWidget->setXml(xml);
        txtWidget->createImage(url);
        pix = txtWidget->thumbnail(50, 40);
        delete txtWidget;
    }

    DocClipBase *clip;
    if (clipId == -1) clip = new DocClipTextFile( name, description, duration, xml, url, pix, alphaTransparency, m_clipCounter++);
    else {
        clip = new DocClipTextFile( name, description, duration, xml, url, pix, alphaTransparency, clipId);
        if (clipId>=m_clipCounter) m_clipCounter = clipId+1;
    }
    m_clipList.append(clip);
    //m_render->getImage(m_clipCounter, description, 12, 64, 50);
    emit clipListUpdated();

    return clip;
}

void ClipManager::editTextClip(DocClipRef * clip, const GenTime & duration, const QString & name,
                               const QString & description, const QDomDocument &xml, const KURL url, const QPixmap &pix, bool alphaTransparency)
{
    clip->setDescription(description);
    clip->setCropDuration(duration);
   
    DocClipTextFile *avClip =
            dynamic_cast < DocClipTextFile * >(clip->referencedClip());
    if (avClip) {
        avClip->setName(name);
        avClip->setDuration(duration);
        avClip->setFileURL(url);
        avClip->setThumbnail(pix);
        avClip->setTextClipXml(xml);
        avClip->setAlpha(alphaTransparency);
        m_render->getImage(url, 50, 40);
        if (clip->numReferences() > 0) emit updateClipThumbnails(clip->referencedClip());
    }
    emit clipListUpdated();
}

void ClipManager::editColorClip(DocClipRef * clip, const QString & color,
    const GenTime & duration, const QString & name,
    const QString & description)
{
    clip->setDescription(description);
    clip->setCropDuration(duration);

    DocClipAVFile *avClip =
	dynamic_cast < DocClipAVFile * >(clip->referencedClip());
    if (avClip) {
	avClip->setName(name);
	avClip->setDuration(duration);
        if (avClip->color() != color) {
            avClip->setColor(color);
            m_render->getImage(avClip->getId(), color, 50, 40);
            if (clip->numReferences() > 0) emit updateClipThumbnails(clip->referencedClip());
        }
    }
    emit clipListUpdated();
}

void ClipManager::editImageClip(DocClipRef * clip, const KURL & file,
    const QString & extension, const int &ttl, const GenTime & duration,
    const QString & description, bool alphaTransparency)
{
    clip->setDescription(description);
    clip->setCropDuration(duration);
    if (ttl != 0) clip->referencedClip()->toDocClipAVFile()->setClipTtl(ttl);
    DocClipAVFile *avClip =
	dynamic_cast < DocClipAVFile * >(clip->referencedClip());
    if (avClip) {
        avClip->setAlpha(alphaTransparency);
	avClip->setDuration(duration);
        if (avClip->fileURL() != file) {
            avClip->setFileURL(file);
            m_render->getImage(file, 50, 40);
            if (clip->numReferences() > 0) emit updateClipThumbnails(clip->referencedClip());
        }
    }
    emit clipListUpdated();
}


void ClipManager::editClip(DocClipRef * clip, const KURL & file, const QString & description)
{
    DocClipAVFile *avClip =
	dynamic_cast < DocClipAVFile * >(clip->referencedClip());
    if (avClip) {
        clip->setDescription(description);
        if (avClip->fileURL() != file) {
            avClip->setFileURL(file);
            emit getFileProperties(file);
            if (clip->numReferences() > 0)  {
                emit fixClipDuration(clip->referencedClip());
                emit updateClipThumbnails(clip->referencedClip());
            }
        }
    }
}



DocClipBase *ClipManager::findClip(const QDomElement & clip)
{
    DocClipBase *result = NULL;
    QPtrListIterator < DocClipBase > itt(m_clipList);
    while (itt.current()) {
	if (itt.current()->matchesXML(clip)) {
	    result = itt.current();
	}
	++itt;
    }

    return result;
}

DocClipBase *ClipManager::findClipById(uint id)
{
    DocClipBase *result = NULL;

    QPtrListIterator < DocClipBase > itt(m_clipList);
    while (itt.current()) {
        if (itt.current()->getId() == id) {
	    result = itt.current();
            break;
	}
	++itt;
    }

    return result;
}

QDomDocument ClipManager::producersList()
{
    QDomDocument sceneList;

    QPtrListIterator < DocClipBase > itt(m_clipList);
    
    QDomElement producer = sceneList.createElement("producer");
    producer.setAttribute("id", "black");
    producer.setAttribute("mlt_service", "colour");
    sceneList.appendChild(producer);
    
    while (itt.current()) {
        if (itt.current()->isDocClipAVFile())
        {
        DocClipAVFile *avClip = itt.current()->toDocClipAVFile();
        if (avClip) {

	    if (avClip->clipType() == DocClipBase::IMAGE) {
		QDomElement producer = sceneList.createElement("producer");
		producer.setAttribute("id",
		    QString("producer") +
		    QString::number(avClip->getId()));
                double ratio = ((double) KdenliveSettings::defaultwidth()/KdenliveSettings::defaultheight())/((double)avClip->clipWidth()/avClip->clipHeight()) * m_render->consumerRatio();
                producer.setAttribute("aspect_ratio", QString::number(ratio));
		producer.setAttribute("resource",
		    avClip->fileURL().path());
		producer.setAttribute("hide", "audio");
		if (avClip->clipTtl()!=0) producer.setAttribute("ttl", QString::number(avClip->clipTtl()));
		sceneList.appendChild(producer);
	    }

	    else if (avClip->clipType() == DocClipBase::COLOR) {
		QDomElement producer = sceneList.createElement("producer");
		producer.setAttribute("id",
		    QString("producer") +
		    QString::number(avClip->getId()));
		producer.setAttribute("mlt_service", "colour");
		producer.setAttribute("hide", "audio");
		producer.setAttribute("colour", avClip->color());
		sceneList.appendChild(producer);
	    }

	    else if (avClip->clipType() == DocClipBase::AUDIO) {
		QDomElement producer = sceneList.createElement("producer");
		producer.setAttribute("id",
		    QString("producer") +
		    QString::number(avClip->getId()));
		producer.setAttribute("resource",
		    avClip->fileURL().path());
		producer.setAttribute("hide", "video");
		sceneList.appendChild(producer);
	    } else if (avClip->clipType() == DocClipBase::VIDEO) {
		QDomElement producer = sceneList.createElement("producer");
		producer.setAttribute("id",
		    QString("producer") +
		    QString::number(avClip->getId()));
		producer.setAttribute("resource",
		    avClip->fileURL().path());
		producer.setAttribute("hide", "audio");
		sceneList.appendChild(producer);
	    }

	    else {
		QDomElement producer = sceneList.createElement("producer");
		producer.setAttribute("id",
		    QString("producer") +
		    QString::number(avClip->getId()));
		producer.setAttribute("resource",
		    avClip->fileURL().path());
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
	       } */
	}
        }
        else if (itt.current()->isDocClipTextFile())
        {
            DocClipTextFile *avClip = itt.current()->toDocClipTextFile();
            if (avClip)
            {
                QDomElement producer = sceneList.createElement("producer");
                producer.setAttribute("id",
                                      QString("producer") + QString::number(avClip->getId()));
                producer.setAttribute("aspect_ratio", QString::number(m_render->consumerRatio()));
                producer.setAttribute("resource", avClip->fileURL().path());
                producer.setAttribute("hide", "audio");
                sceneList.appendChild(producer);
                
            /*QDomElement producer = sceneList.createElement("producer");
            producer.setAttribute("id", QString("producer") + QString::number(avClip->getId()));
            producer.setAttribute("resource","pango");
            producer.setAttribute("bgcolour", "0x000000ff");
            producer.setAttribute("markup", avClip->description());
                sceneList.appendChild(producer);*/
            }
        }
	++itt;
    }
    return sceneList;
}

DocClipBase *ClipManager::findClip(const KURL & file)
{
    DocClipBase *result = 0;
    DocClipAVFile *avClip;

    QPtrListIterator < DocClipBase > itt(m_clipList);
    while (itt.current()) {
        if (itt.current()->isDocClipAVFile()) {
	   avClip = itt.current()->toDocClipAVFile();
  	   if (avClip && (avClip->fileURL() == file)) {
	       result = avClip;
	       break;
           }
	}
	++itt;
    }

    return result;
}

DocClipBase *ClipManager::insertClip(const QDomElement & clip)
{
    DocClipBase *result = findClip(clip);
    if (!result) {
	result =
	    DocClipBase::createClip(m_render->effectList(), *this, clip);
	if (result) {
	    m_clipList.append(result);
	} else {
	    kdError() <<
		"Could not insert clip into clip manager - createClip failed"
		<< endl;
	}
    }
    return result;
}

void ClipManager::clear()
{
#warning "This might blow up spectacularly - this implementation does not check"
#warning "and clean up any references to said clips."
    m_clipCounter = 0;
    m_clipList.clear();
    m_temporaryClipList.clear();
}

DocClipAVFile *ClipManager::findAVFile(const KURL & url)
{
    DocClipAVFile *file = NULL;

    QPtrListIterator < DocClipBase > itt(m_clipList);

    while (itt.current()) {
	DocClipBase *clip = itt.current();
        DocClipAVFile *avClip;
	if (clip->isDocClipAVFile()) {
	    avClip = clip->toDocClipAVFile();

            if (avClip && avClip->fileURL() == url) {
		file = avClip;
		break;
	    }
	}
	++itt;
    }

    return file;
}


void ClipManager::AVFilePropertiesArrived(const QMap < QString,
    QString > &properties)
{
    if (!properties.contains("filename")) {
	kdError() << "File properties returned with no file name attached"
	    << endl;
	return;
    }

    DocClipAVFile *file = findAVFile(KURL(properties["filename"]));
    if (!file) {
	kdWarning() << "File properties returned for a non-existant AVFile"
	    << endl;
	return;
    }

    file->calculateFileProperties(properties);
    if (file->clipType() == DocClipBase::AV || file->clipType() == DocClipBase::AUDIO) {
	connect(file->thumbCreator, SIGNAL(audioThumbReady(QMap<int,QMap<int,QByteArray> >)), file, SLOT(updateAudioThumbnail(QMap<int,QMap<int,QByteArray> >)));
	if (KdenliveSettings::audiothumbnails()) 
		QTimer::singleShot(1000, file, SLOT(getAudioThumbs()));
    }
    emit clipChanged(file);
}


void ClipManager::generateFromXML(KRender * render, const QDomElement & e)
{
    m_clipList.generateFromXML(render->effectList(), *this, render, e);
}

QDomDocument ClipManager::toXML(const QString & element)
{
    return m_clipList.toXML(element);
}

void ClipManager::removeClip(const KURL & file)
{
#warning - to be written
}

void ClipManager::removeClip(int clipId)
{
    //kdDebug()<<"++++  TRYING to delete clip: "<<clipId<<endl;
    DocClipBase *clip = findClipById(clipId);
    if (clip) {
	if (m_clipList.find(clip)!=-1) {
		disconnect(clip->toDocClipAVFile()->thumbCreator, SIGNAL(audioThumbReady(QMap<int,QMap<int,QByteArray> >)), clip->toDocClipAVFile(), SLOT(updateAudioThumbnail(QMap<int,QMap<int,QByteArray> >)));
		m_clipList.remove();
	}
    }
    else kdDebug()<<"++++  CLIP NOT FOUND!"<<endl;
}


void ClipManager::removeClip(const QDomElement & clip)
{
#warning - to be written
}

/** Adds a temporary clip. This is a clip that does not "exist" in the project, but of which
 * some stored information is required. */
DocClipBase *ClipManager::addTemporaryClip(const QDomElement & clip)
{
#warning - to be written.
    return insertClip(clip);
}

DocClipBase *ClipManager::addTemporaryClip(const KURL & file)
{
#warning - to be written.
    return insertClip(file);
}

void ClipManager::AVImageArrived(int id, const QPixmap & pixmap)
{
    DocClipBase *clip = findClipById(id);
    if (clip) {
	clip->setThumbnail(pixmap);
    }
}

void ClipManager::AVImageArrived(const KURL & url, int frame,
    const QPixmap & pixmap)
{
    DocClipBase *clip = findClip(url);

    if (clip) {
	clip->setThumbnail(pixmap);
    }
}
