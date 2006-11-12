/***************************************************************************
                          clipdrag.cpp  -  description
                             -------------------
    begin                : Mon Apr 15 2002
    copyright            : (C) 2002 by Jason Wood
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
#include <kdebug.h>
#include <kmessagebox.h>
#include <klocale.h>

#include "clipdrag.h"
#include "docclipavfile.h"
#include "docclipreflist.h"
#include "clipmanager.h"

#include <iostream>

ClipDrag::ClipDrag(DocClipBase * clip, QWidget * dragSource,
    const char *name):KURLDrag(ClipDrag::createURLList(clip), dragSource,
    name)
{
    m_xml = clip->toXML().toString();

//    kdWarning() << "XML is " << m_xml << endl;
}

ClipDrag::ClipDrag(DocClipRef * clip, QWidget * dragSource,
    const char *name):KURLDrag(ClipDrag::createURLList(clip), dragSource,
    name)
{
    m_xml = clip->toXML().toString();
//    kdWarning() << "XML is " << m_xml << endl;
}

ClipDrag::ClipDrag(DocClipRefList & clips, QWidget * dragSource,
    const char *name):KURLDrag(ClipDrag::createURLList(&clips), dragSource,
    name)
{
    m_xml = clips.toXML("cliplist").toString();
//    kdWarning() << "XML is " << m_xml << endl;
}

ClipDrag::~ClipDrag()
{
}

const char *ClipDrag::format(int i) const
{
    switch (i) {
    case 0:
	return "application/x-kdenlive-clip";
    default:
	return KURLDrag::format(i - 1);
    }
}

QByteArray ClipDrag::encodedData(const char *mime) const
{
    QCString mimetype(mime);

    if (mimetype == "application/x-kdenlive-clip") {
	QByteArray encoded;
	encoded.resize(m_xml.length() + 1);
	memcpy(encoded.data(), m_xml.utf8().data(), m_xml.length() + 1);
	return encoded;
    } else {
	return KURLDrag::encodedData(mime);
    }
}

bool ClipDrag::canDecode(const QMimeSource * mime, bool onlyExternal)
{
    if (mime->provides("application/x-kdenlive-clip")) {
	if (onlyExternal) return false;
	return true;
    }

    return KURLDrag::canDecode(mime);
}

DocClipRefList ClipDrag::decode(const EffectDescriptionList & effectList,
    ClipManager & clipManager, const QMimeSource * e)
{
    DocClipRefList cliplist;

    if (e->provides("application/x-kdenlive-clip")) {
	QByteArray data = e->encodedData("application/x-kdenlive-clip");
	QString xml = data;
	QDomDocument qdomdoc;
	qdomdoc.setContent(data);

	QDomElement elem = qdomdoc.documentElement();
	QDomNode node;

	/*kdDebug() << "DRAG INSIDE-----------------------------------------"
	    << endl;
	kdDebug() << qdomdoc.toString() << endl;
	kdDebug() << "DRAG INSIDE-----------------------------------------"
	    << endl;*/

	// are we handling a single clip, or a clip list? Not sure if both cases will
	// occur, but just in case, we check for it.
	if (elem.tagName() == "cliplist") {
	    node = elem.firstChild();
	} else {
	    node = elem;
	}
	while (!node.isNull()) {
	    QDomElement element = node.toElement();

	    if (!element.isNull()) {
		if (element.tagName() == "kdenliveclip") {
		    DocClipRef *ref =
			DocClipRef::createClip(effectList, clipManager,
			element);
		    cliplist.append(ref);

		    if (element.attribute("master", "false") == "true") {
			cliplist.setMasterClip(ref);
		    }
		}
	    }
	    node = node.nextSibling();
	}
    } else {
	KURL::List list;
	KURL::List::Iterator it;
	KURLDrag::decode(e, list);
	for (it = list.begin(); it != list.end(); ++it) {
	    DocClipBase *file = clipManager.findClip(*it);
	    if (!file) {
		file = clipManager.addTemporaryClip(*it);
	        if (file) {
		    DocClipRef *refFile = new DocClipRef(file);
	            cliplist.append(refFile);
		}
	    }
	    else KMessageBox::sorry(0, i18n("The clip %1 is already present in this project").arg((*it).filename()));
	}
    }

    return cliplist;
}

KURL::List ClipDrag::createURLList(DocClipRefList * clipList)
{
    KURL::List list;

    QPtrListIterator < DocClipRef > itt(*clipList);

    while (itt.current() != 0) {
	list.append(itt.current()->fileURL());
	++itt;
    }

    return list;
}

KURL::List ClipDrag::createURLList(DocClipRef * clip)
{
    KURL::List list;

    list.append(clip->fileURL());
    return list;
}

KURL::List ClipDrag::createURLList(DocClipBase * clip)
{
    KURL::List list;

    list.append(clip->fileURL());
    return list;
}
