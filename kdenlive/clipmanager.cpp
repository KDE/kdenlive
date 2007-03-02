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
#include <qimage.h>
#include <qcheckbox.h>

// include files for KDE
#include <klocale.h>
#include <kdebug.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kio/netaccess.h>

#include <docclipbase.h>
#include <docclipavfile.h>
#include <doccliptextfile.h>
#include <docclipvirtual.h>
#include <krendermanager.h>
#include <kaddclipcommand.h>
#include "titlewidget.h"
#include "timecode.h"

ClipManager::ClipManager(KRender *render, QWidget * parent, const char *name) //Manager & renderManager
{
    m_clipList.setAutoDelete(true);
    m_temporaryClipList.setAutoDelete(true);
    m_clipCounter = 0;

    m_render = render;

    connect(m_render, SIGNAL(replyGetFileProperties(const QMap < QString,
		QString > &, const QMap < QString,
		QString > &)), this,
	SLOT(AVFilePropertiesArrived(const QMap < QString, QString > &, const QMap < QString,
		QString > &)));

    connect(m_render, SIGNAL(replyGetImage(const KURL &, int,
		const QPixmap &, int, int)), this,
	SLOT(AVImageArrived(const KURL &, int, const QPixmap &)));

    connect(m_render, SIGNAL(replyGetImage(int, const QPixmap &, int,
		int)), this, SLOT(AVImageArrived(int, const QPixmap &)));
    
    connect(this, SIGNAL(getFileProperties(KURL, uint)), m_render, SLOT(getFileProperties(KURL, uint)));
}

ClipManager::~ClipManager()
{
}

KURL ClipManager::checkFileUrl(KURL url)
{
	if (!m_relocateUrl.isEmpty()) {
	    KURL testUrl = KURL(m_relocateUrl + url.fileName());
	    if (KIO::NetAccess::exists(testUrl, true, 0)) return testUrl;
	}
	if (KMessageBox::questionYesNo(0, i18n("Cannot open file %1\nDo you want to search for the file or remove it from the project ?").arg(url.path()), i18n("Missing File"), i18n("Find File"), i18n("Remove")) == KMessageBox::Yes) {
	    url = KFileDialog::getOpenURL(url.path());
	    m_relocateUrl = url.directory();
	    return url;
	}
	return KURL();
}

DocClipBase *ClipManager::insertClip(KURL file, int thumbnailFrame, int clipId)
{
    if (!KIO::NetAccess::exists(file, true, 0)) {
	file = checkFileUrl(file);
	if (file.isEmpty()) return 0;
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
            if (clipId>= (int) m_clipCounter) m_clipCounter = clipId+1;
        }
	clip->setProjectThumbFrame(thumbnailFrame);
	m_clipList.append(clip);
        emit getFileProperties(file, thumbnailFrame);
	emit clipListUpdated();
	return clip;
    }
    return 0;
}

QDomDocument ClipManager::buildClip(KURL & file, int clipId)
{
    if (!KIO::NetAccess::exists(file, true, 0)) {
	file = checkFileUrl(file);
	if (file.isEmpty()) return QDomDocument();
    }

    DocClipBase *clip = findClip(file);
    if (clip) return clip->toXML();

	if (!m_render->isValid(file)) {
	    KMessageBox::sorry(0,
		i18n
		("The file %1 is not a valid video file for kdenlive...").
		arg(file.filename()));
	    return QDomDocument();
	}

        if (clipId == -1) clip = new DocClipAVFile(file.fileName(), file, m_clipCounter++);
        else {
            clip = new DocClipAVFile(file.fileName(), file, clipId);
        }
	QDomDocument xml = clip->toXML();
	delete clip;
	return xml;
}


DocClipBase *ClipManager::insertImageClip(KURL file,
    const GenTime & duration, const QString & description, bool alphaTransparency, int clipId)
{
    if (!KIO::NetAccess::exists(file, true, 0)) {
	file = checkFileUrl(file);
	if (file.isEmpty()) return 0;
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

    	if (clipId == -1) clip = new DocClipAVFile(file, duration, alphaTransparency, m_clipCounter++);
    	else {
        	clip = new DocClipAVFile(file, duration, alphaTransparency, clipId);
          if (clipId>= (int) m_clipCounter) m_clipCounter = clipId+1;
    	}
    	clip->setDescription(description);
    	m_clipList.append(clip);
    	m_render->getImage(file, 50, 40);
    	emit clipListUpdated();
	return clip;
    }
    return 0;
}

QDomDocument ClipManager::buildImageClip(KURL file,
    const GenTime & duration, const QString & description, bool alphaTransparency, int clipId)
{

    if (!KIO::NetAccess::exists(file, true, 0)) {
	file = checkFileUrl(file);
	if (file.isEmpty()) return QDomDocument();
    }

    DocClipBase *clip = findClip(file);
    if (clip) return clip->toXML();
	if (!m_render->isValid(file)) {
	    KMessageBox::sorry(0,
		i18n
		("The file %1 is not a valid video file for kdenlive...").
		arg(file.filename()));
	    return QDomDocument();
	}

    	if (clipId == -1) clip = new DocClipAVFile(file, duration, alphaTransparency, m_clipCounter++);
    	else {
        	clip = new DocClipAVFile(file, duration, alphaTransparency, clipId);
    	}
    	clip->setDescription(description);
    	
	QDomDocument xml = clip->toXML();
	delete clip;
	return xml;
}

DocClipBase *ClipManager::insertSlideshowClip(const KURL & file,
    const QString & extension, const int &ttl, bool crossfade, const QString &lumaFile, double lumasoftness, uint lumaduration, const GenTime & duration,
    const QString & description, bool alphaTransparency, int clipId)
{
    DocClipBase *clip = findClip(file);
    if (!clip) {
	if (!m_render->isValid(file)) {
	    KMessageBox::sorry(0,
		i18n
		("The file %1 is not a valid video file for kdenlive...").
		arg(file.filename()));
	    return 0;
	}

    	if (clipId == -1) clip = new DocClipAVFile(file, extension, ttl, duration, alphaTransparency, crossfade, lumaFile, lumasoftness, lumaduration, m_clipCounter++);
    	else {
        	clip = new DocClipAVFile(file, extension, ttl, duration, alphaTransparency, crossfade, lumaFile, lumasoftness, lumaduration, clipId);
          if (clipId>= (int) m_clipCounter) m_clipCounter = clipId+1;
    	}
	int imageCount = (int) duration.frames(KdenliveSettings::defaultfps()) / ttl;
	clip->setName(clip->name() + i18n(" [%1 images]").arg(QString::number(imageCount)));
	
    	clip->setDescription(description);
    	m_clipList.append(clip);

    	m_render->getImage(file, 50, 40);

    	emit clipListUpdated();
	return clip;
    }
    return 0;
}


QDomDocument ClipManager::buildSlideshowClip(const KURL & file,
    const QString & extension, const int &ttl, bool crossfade, const QString &lumaFile, double lumasoftness, uint lumaduration, const GenTime & duration,
    const QString & description, bool alphaTransparency, int clipId)
{
    DocClipBase *clip = findClip(file);
    if (!clip) {
	if (!m_render->isValid(file)) {
	    KMessageBox::sorry(0,
		i18n
		("The file %1 is not a valid video file for kdenlive...").
		arg(file.filename()));
	    return QDomDocument();
	}

    	if (clipId == -1) clip = new DocClipAVFile(file, extension, ttl, duration, alphaTransparency, crossfade, lumaFile, lumasoftness, lumaduration, m_clipCounter++);
    	else {
        	clip = new DocClipAVFile(file, extension, ttl, duration, alphaTransparency, crossfade, lumaFile, lumasoftness, lumaduration, clipId);
    	}
	int imageCount = (int) duration.frames(KdenliveSettings::defaultfps()) / ttl;
	clip->setName(clip->name() + i18n(" [%1 images]").arg(QString::number(imageCount)));
    	clip->setDescription(description);
	QDomDocument xml = clip->toXML();
	delete clip;
	return xml;
    }
    return QDomDocument();
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

QDomDocument ClipManager::buildColorClip(const QString & color,
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
    QDomDocument xml = clip->toXML();
    delete clip;
    return xml;
}

QDomDocument ClipManager::buildTextClip(const GenTime & duration, const QString & name,
    const QString & description, const QDomDocument &xml, const KURL url, QPixmap &pix, bool alphaTransparency, int clipId)
{
    DocClipBase *clip;
    if (clipId == -1) clip = new DocClipTextFile( name, description, duration, xml, url, 0, alphaTransparency, m_clipCounter++);
    else {
        clip = new DocClipTextFile( name, description, duration, xml, url, 0, alphaTransparency, clipId);
    }

    QDomDocument resultxml = clip->toXML();
    delete clip;
    return resultxml;
}

QDomDocument ClipManager::buildVirtualClip(const GenTime & start, const GenTime & end, const QString & name, const QString & description, const KURL url, int clipId)
{
    DocClipBase *clip;
    if (clipId == -1) clip = new DocClipVirtual( url, name, description, start, end, m_clipCounter++);
    else {
        clip = new DocClipVirtual(url, name, description, start, end, clipId);
    }
    QDomDocument resultxml = clip->toXML();
    delete clip;
    return resultxml;
}

DocClipBase *ClipManager::insertXMLClip(QDomDocument node)
{
    DocClipAVFile *clip = new DocClipAVFile(node);
    m_clipList.append(clip);
    m_clipCounter++;
    switch (clip->clipType()) {
    case DocClipBase::COLOR:
	m_render->getImage(clip->getId(), clip->color(), 50, 40);
	break;
    case DocClipBase::IMAGE:
	m_render->getImage(clip->fileURL(), 50, 40);
	break;
    case DocClipBase::SLIDESHOW:
	m_render->getImage(clip->fileURL(), 50, 40);
	break;
    default:
	emit getFileProperties(clip->fileURL(), clip->getProjectThumbFrame());
	//m_render->getImage(clip->fileURL(), 1, 50, 40);//
	break;
    }
    if (clip->getId() >= m_clipCounter) m_clipCounter = clip->getId() + 1;
    emit clipListUpdated();
    return clip;
}

DocClipBase *ClipManager::insertXMLTextClip(QDomDocument node)
{
    DocClipBase *clip;
    clip = new DocClipTextFile(node);
    m_clipList.append(clip);
    QPixmap result(50, 40);
    QPixmap pix(48,38);
    result.fill(Qt::black);
    if (!QFile(clip->fileURL().path()).exists()) {
            titleWidget *txtWidget=new titleWidget(NULL,10,10);
            txtWidget->setXml(clip->toDocClipTextFile()->textClipXml());
	    KTempFile tmp(KdenliveSettings::currenttmpfolder(),".png");
	    txtWidget->transparentTitle->setChecked(clip->toDocClipTextFile()->isTransparent());
            txtWidget->createImage(KURL(tmp.name()));
	    pix = txtWidget->thumbnail(48, 38);
            delete txtWidget;
    }
    else {
	    QImage im(clip->fileURL().path());
	    pix = im.smoothScale(48,38);
    }
    copyBlt(&result, 1, 1, &pix, 0, 0, 48, 38);
    clip->setThumbnail(result);
    m_clipCounter++;
    emit clipListUpdated();
    return clip;
}

DocClipBase *ClipManager::insertXMLVirtualClip(QDomDocument node)
{
    DocClipBase *clip;
    clip = new DocClipVirtual(node);
    m_clipList.append(clip);
    emit getFileProperties(clip->fileURL(), clip->getProjectThumbFrame());
    m_clipCounter++;
    emit clipListUpdated();
    return clip;
}

DocClipBase *ClipManager::insertVirtualClip(const QString & name,
    const QString & description, const GenTime & start, const GenTime & end, const KURL url, int thumbnailFrame, int clipId)
{
    QPixmap result(50, 40);
    result.fill(Qt::black);

    DocClipBase *clip;
    if (clipId == -1) clip = new DocClipVirtual( url, name, description, start, end, m_clipCounter++);
    else {
        clip = new DocClipVirtual( url, name, description, start, end, clipId);
        if (clipId>=(int) m_clipCounter) m_clipCounter = clipId+1;
    }
    clip->setThumbnail(result);
    m_clipList.append(clip);
    emit clipListUpdated();
    return clip;
}

DocClipBase *ClipManager::insertTextClip(
    const GenTime & duration, const QString & name,
    const QString & description, const QDomDocument &xml, KURL url, QPixmap &pix, bool alphaTransparency, int clipId)
{
    QPixmap result(50, 40);
    result.fill(Qt::black);
    if (!QFile(url.path()).exists()) {
	int width = KdenliveSettings::defaultwidth();
	if (KdenliveSettings::videoprofile() == "dv_wide") width = width * 4 / 3;
        titleWidget *txtWidget=new titleWidget(NULL ,width,KdenliveSettings::defaultheight());
        txtWidget->setXml(xml);
	KTempFile tmp(KdenliveSettings::currenttmpfolder(),".png");
	url = KURL(tmp.name());
	txtWidget->transparentTitle->setChecked(alphaTransparency);
        txtWidget->createImage(url);
        pix = txtWidget->thumbnail(48, 38);
    	copyBlt(&result, 1, 1, &pix, 0, 0, 48, 38);
        delete txtWidget;
    }
    if (pix.isNull()) {
	QImage im(url.path());
	QPixmap pixmap = im.smoothScale(48, 38);
    	copyBlt(&result, 1, 1, &pixmap, 0, 0, 48, 38);
    }

    DocClipBase *clip;
    if (clipId == -1) clip = new DocClipTextFile( name, description, duration, xml, url, result, alphaTransparency, m_clipCounter++);
    else {
        clip = new DocClipTextFile( name, description, duration, xml, url, result, alphaTransparency, clipId);
        if (clipId>=(int) m_clipCounter) m_clipCounter = clipId+1;
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

        QPixmap result(50, 40);
        result.fill(Qt::black);
        QImage im(url.path());
	QPixmap pix = im.smoothScale(48,38);
	copyBlt(&result, 1, 1, &pix, 0, 0, 48, 38);
	clip->referencedClip()->setThumbnail(result);
//        m_render->getImage(url, 50, 40);
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
    const GenTime & duration,
    const QString & description, bool alphaTransparency)
{
    clip->setDescription(description);
    clip->setCropDuration(duration);
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

void ClipManager::editSlideshowClip(DocClipRef * clip, const KURL & file,
    const QString & extension, const int &ttl, bool crossfade, const QString & lumaFile, double lumaSoftness, uint lumaduration, const GenTime & duration, const QString & description, bool alphaTransparency)
{
    clip->setDescription(description);
    clip->setCropDuration(duration);
    clip->referencedClip()->toDocClipAVFile()->setClipTtl(ttl);
    clip->referencedClip()->toDocClipAVFile()->setCrossfade(crossfade);
    clip->referencedClip()->toDocClipAVFile()->setLumaFile(lumaFile);
    clip->referencedClip()->toDocClipAVFile()->setLumaSoftness(lumaSoftness);
    clip->referencedClip()->toDocClipAVFile()->setLumaDuration(lumaduration);
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
            emit getFileProperties(file, clip->referencedClip()->getProjectThumbFrame());
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

QDomDocumentFragment ClipManager::producersList()
{
    QDomDocument sceneList;
    QDomDocumentFragment list = sceneList.createDocumentFragment();
    sceneList.appendChild(list);

    QPtrListIterator < DocClipBase > itt(m_clipList);
    QDomElement producer = sceneList.createElement("producer");
    producer.setAttribute("id", "black");
    producer.setAttribute("mlt_service", "colour");
    list.appendChild(producer);
    
    while (itt.current()) {
        if (itt.current()->isDocClipAVFile())
        {
        DocClipAVFile *avClip = itt.current()->toDocClipAVFile();
        if (avClip && avClip->numReferences() > 0) {
	    if (avClip->clipType() == DocClipBase::IMAGE || avClip->clipType() == DocClipBase::SLIDESHOW) {
		QDomElement producer = sceneList.createElement("producer");
		producer.setAttribute("id", avClip->getId());
                double ratio = ((double) KdenliveSettings::defaultwidth()/KdenliveSettings::defaultheight())/((double)avClip->clipWidth()/avClip->clipHeight()) * m_render->consumerRatio();
                if (KdenliveSettings::distortimages()) producer.setAttribute("aspect_ratio", QString::number(ratio));
		producer.setAttribute("resource", avClip->fileURL().path());
		producer.setAttribute("ttl", avClip->clipTtl());
		producer.setAttribute("hide", "audio");
		list.appendChild(producer);
	    }

	    else if (avClip->clipType() == DocClipBase::COLOR) {
		QDomElement producer = sceneList.createElement("producer");
		producer.setAttribute("id", avClip->getId());
		producer.setAttribute("mlt_service", "colour");
		producer.setAttribute("hide", "audio");
		producer.setAttribute("colour", avClip->color());
		list.appendChild(producer);
	    }

	    else if (avClip->clipType() == DocClipBase::AUDIO) {
		QDomElement producer = sceneList.createElement("producer");
		producer.setAttribute("id", avClip->getId());
		producer.setAttribute("resource",
		    avClip->fileURL().path());
		producer.setAttribute("hide", "video");
		list.appendChild(producer);
	    } else if (avClip->clipType() == DocClipBase::VIDEO) {
		QDomElement producer = sceneList.createElement("producer");
		producer.setAttribute("id", avClip->getId());
		producer.setAttribute("resource",
		    avClip->fileURL().path());
		producer.setAttribute("hide", "audio");
		list.appendChild(producer);
	    }
	    else {
		QDomElement producer = sceneList.createElement("producer");
		producer.setAttribute("id", avClip->getId());
		producer.setAttribute("resource",
		    avClip->fileURL().path());
		list.appendChild(producer);
	    }
	}
        }
        else if (itt.current()->isDocClipTextFile())
        {
            DocClipTextFile *avClip = itt.current()->toDocClipTextFile();
            if (avClip)
            {
                QDomElement producer = sceneList.createElement("producer");
                producer.setAttribute("id", avClip->getId());
                producer.setAttribute("aspect_ratio", QString::number(m_render->consumerRatio()));
                producer.setAttribute("resource", avClip->fileURL().path());
                producer.setAttribute("hide", "audio");
                list.appendChild(producer);
            }
        }
	++itt;
    }
    return list;
}

QDomDocumentFragment ClipManager::virtualProducersList()
{
    QDomDocument sceneList;
    QDomDocumentFragment list = sceneList.createDocumentFragment();
    sceneList.appendChild(list);

    QPtrListIterator < DocClipBase > itt(m_clipList);
    while (itt.current()) {
	if (itt.current()->isDocClipVirtual()) {
		DocClipVirtual *avClip = itt.current()->toDocClipVirtual();
		if (avClip) {
		    QDomElement producer = sceneList.createElement("producer");
		    producer.setAttribute("id", avClip->getId());
		    producer.setAttribute("mlt_service", "westley");
		    producer.setAttribute("resource", avClip->fileURL().path());
		    list.appendChild(producer);
		}
	}
	++itt;
    }
    return list;
}

QValueList < QPoint > ClipManager::virtualZones()
{
    QValueList < QPoint > list;
    QPtrListIterator < DocClipBase > itt(m_clipList);
    while (itt.current()) {
	if (itt.current()->isDocClipVirtual()) {
		DocClipVirtual *avClip = itt.current()->toDocClipVirtual();
		if (avClip) {
		    list.append(QPoint(avClip->virtualStartTime().frames(KdenliveSettings::defaultfps()), avClip->virtualEndTime().frames(KdenliveSettings::defaultfps())));
		}
	}
	++itt;
    }
    return list;
}

DocClipBase *ClipManager::findClip(const KURL & file)
{
    DocClipBase *result = 0;
    DocClipAVFile *avClip;
    QPtrListIterator < DocClipBase > itt(m_clipList);
    while (itt.current()) {
	if (itt.current()->fileURL().path() == file.path()) {
		result = itt.current();
		break;
	} 
	++itt;
    }
    return result;
}

DocClipBase *ClipManager::insertClip(const QDomElement & clip)
{
    DocClipBase *result = findClip(clip);
    if (!result) {
	result = DocClipBase::createClip(m_render->effectList(), *this, clip);
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

DocClipBase *ClipManager::insertXMLClip(const QDomElement & clip)
{
    DocClipBase *tmp = DocClipBase::createClip(m_render->effectList(), *this, clip);
    if (tmp) {
	    int clipId = tmp->getId();
            DocClipBase *result = new DocClipAVFile(tmp->name(), tmp->fileURL(), clipId);
            if (clipId>= (int) m_clipCounter) m_clipCounter = clipId+1;
	m_clipList.append(result);
        emit getFileProperties(tmp->fileURL(), tmp->getProjectThumbFrame());
	emit clipListUpdated();
	} else {
	    kdError() <<
		"Could not insert clip into clip manager - createClip failed"
		<< endl;
	}
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
	if (clip->isDocClipAVFile()) {
	    DocClipAVFile *avClip = clip->toDocClipAVFile();
            if (avClip && avClip->fileURL().path() == url.path()) {
		file = avClip;
		break;
	    }
	}
	++itt;
    }
    return file;
}


void ClipManager::AVFilePropertiesArrived(const QMap < QString,
    QString > &properties, const QMap < QString,
    QString > &metadata)
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

    file->calculateFileProperties(properties, metadata);
    if ((file->clipType() == DocClipBase::AV || file->clipType() == DocClipBase::AUDIO) && file->thumbCreator) {
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
    kdDebug()<<"++++  TRYING to delete clip: "<<clipId<<endl;
    DocClipBase *clip = findClipById(clipId);
    if (clip) {
	if (m_clipList.find(clip)!=-1) {
		/*disconnect(clip->toDocClipAVFile()->thumbCreator, SIGNAL(audioThumbReady(QMap<int,QMap<int,QByteArray> >)), clip->toDocClipAVFile(), SLOT(updateAudioThumbnail(QMap<int,QMap<int,QByteArray> >)));*/
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
    if (!m_render->isValid(file)) {
	    KMessageBox::sorry(0, i18n("The file %1 is not a valid video file for kdenlive...").arg(file.filename()));
	    return 0;
    }

    // check if dropped file is an image
    KMimeType::Ptr type = KMimeType::findByURL(file);
    if (type->name().startsWith("image/")) {
	QString dur = KdenliveSettings::colorclipduration();
    	Timecode tc;
    	if (KdenliveSettings::defaultfps() == 30000.0 / 1001.0 ) tc.setFormat(30, true);
    	else tc.setFormat(KdenliveSettings::defaultfps());
    	int frames = tc.getFrameNumber(dur, KdenliveSettings::defaultfps());
	return new DocClipAVFile(file, GenTime(frames , KdenliveSettings::defaultfps()), false, m_clipCounter++);
    }
    else return new DocClipAVFile(file.fileName(), file, m_clipCounter++);
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
