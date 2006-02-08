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
#include "loadprojectnativefilter.h"

#include <qdom.h>
#include <qfile.h>

#include <kdebug.h>
#include <klocale.h>
#include <kurl.h>

#include "clipmanager.h"
#include "docclipproject.h"
#include "documentclipnode.h"
#include "kdenlivedoc.h"

LoadProjectNativeFilter::LoadProjectNativeFilter()
:  LoadProjectFilter()
{
}


LoadProjectNativeFilter::~LoadProjectNativeFilter()
{
}

// virtual
bool LoadProjectNativeFilter::load(QFile & file, KdenliveDoc * document)
{
    bool avListLoaded = false;
    bool trackListLoaded = false;

    QDomDocument doc;
    doc.setContent(&file, false);

    QDomElement documentElement = doc.documentElement();

    if (documentElement.tagName() != "kdenlivedoc") {
	kdWarning() <<
	    "KdenliveDoc::loadFromXML() document element has unknown tagName : "
	    << documentElement.tagName() << endl;
    }

    QDomNode n = documentElement.firstChild();

    while (!n.isNull()) {
	QDomElement e = n.toElement();
	if (!e.isNull()) {
	    if ((e.tagName() == "AVFileList")
		|| (e.tagName() == "avfilelist")) {
		if (!avListLoaded) {
		    avListLoaded = true;
		    loadAVFileList(e, document);
		} else {
		    kdWarning() <<
			"Second AVFileList discovered, skipping..." <<
			endl;
		}
	    } else if (e.tagName() == "clip") {
		if (!trackListLoaded) {
		    trackListLoaded = true;
		    loadTrackList(e, document);
		} else {
		    kdWarning() <<
			"Second timeline discovered, skipping..." << endl;
		}
	    } else {
		kdWarning() << "Unknown tag " << e.
		    tagName() << ", skipping..." << endl;
	    }
	}
	n = n.nextSibling();
    }

    return true;
}

// virtual
QStringList LoadProjectNativeFilter::handledFormats() const
{
    QStringList list;
    list.append("application/vnd.kde.kdenlive");
    return list;
}

void LoadProjectNativeFilter::loadAVFileList(QDomElement & element,
    KdenliveDoc * document)
{
    QDomNode n = element.firstChild();

    while (!n.isNull()) {
	QDomElement e = n.toElement();
	if (!e.isNull()) {
	    addToDocument(i18n("Clips"), e, document);
	}
	n = n.nextSibling();
    }
}

void LoadProjectNativeFilter::loadTrackList(QDomElement & element,
    KdenliveDoc * document)
{
    DocClipBase *clip =
	DocClipBase::createClip(document->effectDescriptions(),
	document->clipManager(), element);

    if (clip) {
	if (clip->isProjectClip()) {
	    document->setProjectClip(dynamic_cast <
		DocClipProject * >(clip));
	} else {
	    delete clip;
	    kdError() <<
		"Base clip detected, not a project clip. Ignoring..." <<
		endl;
	}
    } else {
	kdError() << "Could not create clip for track list, ignoring..." <<
	    endl;
    }
}

void LoadProjectNativeFilter::addToDocument(const QString & parent,
    QDomElement & clip, KdenliveDoc * document)
{
    DocumentBaseNode *parentNode = document->findClipNode(parent);
    DocumentBaseNode *thisNode = 0;

    if (parentNode) {
	if ((clip.tagName() == "AVFile") || (clip.tagName() == "avfile")) {
	    uint clipType;
	    clipType = clip.attribute("type", "").toInt();
	    DocClipBase *baseClip;

	    if (clipType < 4)	//  AUDIO OR VIDEO CLIP
		baseClip =
		    document->clipManager().insertClip(clip.
		    attribute("url", ""));

	    else if (clipType == DocClipBase::COLOR)	//   COLOR CLIP
		baseClip =
		    document->clipManager().insertColorClip(clip.
		    attribute("color", ""),
		    GenTime(clip.attribute("duration", "").toInt(), 25),
		    clip.attribute("name", ""),
		    clip.attribute("description", ""));

	    else if (clipType == DocClipBase::IMAGE)	//   IMAGE CLIP
		baseClip =
		    document->clipManager().insertImageClip(clip.
		    attribute("url", ""), "", 0,
		    GenTime(clip.attribute("duration", "").toInt(), 25),
		    clip.attribute("description", ""));
            
            else if (clipType == DocClipBase::TEXT)	//   TEXT CLIP
            {
                QDomDocument xml;
                QPixmap pm = QPixmap();
                xml.setContent(clip.attribute("xml", ""));
                baseClip =
                        document->clipManager().insertTextClip(GenTime(clip.attribute("duration", "").toInt(), 25), clip.attribute("name", ""),
                clip.attribute("description", ""),xml, clip.attribute("url", ""), pm);
            }
            

	    DocumentClipNode *clipNode =
		new DocumentClipNode(parentNode, baseClip);
	    thisNode = clipNode;

	    QString desc;
	    // find description, if one exists.
	    QDomNode n = clip.firstChild();

	    while (!n.isNull()) {
		QDomText t = n.toText();

		if (!t.isNull()) {
		    desc.append(t.nodeValue());
		}

		n = n.nextSibling();
	    }

	    clipNode->clipRef()->setDescription(desc);

	}
    } else {
	kdError() << "Could not find document base node " << parent <<
	    " in document" << endl;
    }

    if (thisNode) {
	document->addClipNode(parent, thisNode);

	QDomNode n = clip.firstChild();

	while (!n.isNull()) {
	    QDomElement e = n.toElement();
	    if (!e.isNull()) {
		addToDocument(thisNode->name(), e, document);
	    }
	    n = n.nextSibling();
	}
    }
}
