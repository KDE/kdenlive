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

#include <qdom.h>
#include <qfile.h>

#include <kdebug.h>
#include <klocale.h>
#include <kstandarddirs.h>
#include <kurl.h>
#include <kio/netaccess.h>

#include "loadprojectnativefilter.h"
#include "clipmanager.h"
#include "docclipproject.h"
#include "documentclipnode.h"
#include "documentgroupnode.h"
#include "kdenlivedoc.h"
#include "kdenlive.h"

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
    bool trackListLoaded = false;
    GenTime inPoint(0.0);
    GenTime outPoint(3.0);
    int currentPos = 0;
    
    QDomDocument doc;
    doc.setContent(&file, false);

    QDomElement documentElement = doc.documentElement();

    if (documentElement.tagName() != "westley") {
	kdWarning() <<
	    "KdenliveDoc::loadFromXML() document element has unknown tagName : "
	    << documentElement.tagName() << endl;
    }
    QDomNode kdenlivedoc = documentElement.elementsByTagName("kdenlivedoc").item(0);
    double version = kdenlivedoc.toElement().attribute("version", "0").toDouble();
    if (version < 0.5) {
	doc = upgradeDocumentFile(doc, document, version);
	documentElement = doc.documentElement();
	kdenlivedoc = documentElement.elementsByTagName("kdenlivedoc").item(0);
    }
    QDomNode n = kdenlivedoc.firstChild();
    QDomElement e;

    while (!n.isNull()) {
	e = n.toElement();
	if (!e.isNull()) {
	    if (e.tagName() == "properties") {
		KdenliveSettings::setCurrentdefaultfolder(e.attribute("projectfolder",""));
		// create a temp folder for previews & thumbnails in KDE's tmp resource dir

		if (KdenliveSettings::userdefinedtmp()) {
			    KdenliveSettings::setCurrenttmpfolder( KdenliveSettings::defaulttmpfolder() + "/kdenlive/" );
			    KIO::NetAccess::mkdir(KURL(KdenliveSettings::currenttmpfolder()), 0, -1);
		}
		else KdenliveSettings::setCurrenttmpfolder(locateLocal("tmp", "kdenlive/", true));

		QStringList meta = QStringList::split("#", e.attribute("metadata", QString::null), true);
		if (!meta.isEmpty()) document->slotSetMetadata( meta );

		KdenliveSettings::setDefaultheight(e.attribute("projectheight","576").toInt());
		KdenliveSettings::setDefaultwidth(e.attribute("projectwidth","720").toInt());
		KdenliveSettings::setDefaultfps(e.attribute("projectfps","25.0").toDouble());
		KdenliveSettings::setAspectratio(e.attribute("projectratio",QString::number(59.0 / 54.0)).toDouble());

		document->application()->setFramesPerSecond();

		currentPos = e.attribute("timeline_position","0").toInt();
		inPoint = GenTime(e.attribute("inpoint","0").toInt(), KdenliveSettings::defaultfps());
		outPoint = GenTime(e.attribute("outpoint","100").toInt(), KdenliveSettings::defaultfps());

	    }
	    else if (e.tagName() == "guides") {
		document->application()->guidesFromXml( e );
	    }
	    else if (e.tagName() == "producer" || e.tagName() == "folder") {
		addToDocument(i18n("Clips"), e, document);
	    }
	    else if (e.tagName() == "kdenliveclip") {
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

    document->application()->setInpointPosition(inPoint);
    document->application()->setOutpointPosition(outPoint);
    // kdDebug()<<" / / Loaded, seek to: "<<currentPos<<endl;
    document->application()->setCursorPosition(GenTime(currentPos, KdenliveSettings::defaultfps()));
    //document->renderer()->seek(GenTime(currentPos, KdenliveSettings::defaultfps()));
    return true;
}


QDomDocument LoadProjectNativeFilter::upgradeDocumentFile(QDomDocument kdenlivedoc, KdenliveDoc * document, double version)
{
    // upgrade from 0.4: replace effects names with locale independant strings
    if (version < 0.5) {
	QDomNodeList list = kdenlivedoc.elementsByTagName("effect");
	uint cnt = list.count();
	uint ix = 0;
	while (ix < cnt) {
	    QDomNode item = list.item(ix);
	    QDomElement e = item.toElement();
	    e.setAttribute("id", document->getEffectStringId(e.attribute("name", "")));
	    ix++;
	}
    }
    return kdenlivedoc;
}

// virtual
bool LoadProjectNativeFilter::merge(QFile & file, KdenliveDoc * document, bool insertTimeLine, GenTime insertTime)
{
    int currentPos = 0;
    bool trackListLoaded = false;
    QDomDocument doc;
    doc.setContent(&file, false);

    QDomElement documentElement = doc.documentElement();

    if (documentElement.tagName() != "kdenlivedoc") {
	kdWarning() <<
	    "KdenliveDoc::loadFromXML() document element has unknown tagName : "
	    << documentElement.tagName() << endl;
    }
    QDomNode kdenlivedoc = documentElement.elementsByTagName("kdenlivedoc").item(0);
    QDomNode n = kdenlivedoc.firstChild();

    while (!n.isNull()) {
	QDomElement e = n.toElement();
	if (!e.isNull()) {
	    if (e.tagName() == "guides") {
		//document->application()->guidesFromXml( e );
	    }
	    else if (e.tagName() == "producer" || e.tagName() == "folder") addToDocument(i18n("Clips"), e, document);
	    else if (insertTimeLine && e.tagName() == "kdenliveclip") {
		if (!trackListLoaded) {
		    trackListLoaded = true;
		    loadTrackList(e, document, insertTime);
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
    KdenliveDoc * document, GenTime insertTime)
{
    DocClipBase *clip =
	DocClipBase::createClip(document, element);
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
    QValueVector < CommentedTime > markers;

    if (parentNode) {
	if ((clip.tagName() == "producer") && clip.attribute("id", QString::null) != "black") {
	    uint clipType;
	    clipType = clip.attribute("type", "").toInt();
	    DocClipBase *baseClip=0;

	    if (clipType < 4)	//  AUDIO OR VIDEO CLIP
	    {
		baseClip =
		    document->clipManager().insertClip(KURL(clip.
                        attribute("resource", "")), clip.attribute("frame_thumbnail", "0").toInt(), clip.attribute("id", "-1").toInt());
	    }
	    else if (clipType == DocClipBase::COLOR)	//   COLOR CLIP
		baseClip =
		    document->clipManager().insertColorClip(clip.
		    attribute("colour", ""),
		    GenTime(clip.attribute("duration", "").toInt(), 25),
		    clip.attribute("name", ""),
                    clip.attribute("description", ""), clip.attribute("id", "-1").toInt());

	    else if (clipType == DocClipBase::IMAGE)	//   IMAGE CLIP
		baseClip =
		    document->clipManager().insertImageClip(clip.
		    attribute("resource", ""), GenTime(clip.attribute("duration", "").toInt(), 25),
                    clip.attribute("description", ""), clip.attribute("transparency", "").toInt(), clip.attribute("id", "-1").toInt());

	    else if (clipType == DocClipBase::SLIDESHOW)	//   SLIDESHOW CLIP
		baseClip =
		    document->clipManager().insertSlideshowClip(clip.
		    attribute("resource", ""), "", clip.attribute("ttl", "0").toInt(), clip.attribute("crossfade","").toInt(), clip.attribute("lumafile",""), clip.attribute("lumasoftness","0").toDouble(), clip.attribute("lumaduration","0").toInt(),
		    GenTime(clip.attribute("duration", "").toInt(), 25),
                    clip.attribute("description", ""), clip.attribute("transparency","").toInt(), clip.attribute("id", "-1").toInt());

            else if (clipType == DocClipBase::TEXT)	//   TEXT CLIP
            {
                QDomDocument xml;
                QPixmap pm = QPixmap();
                xml.appendChild(clip.firstChild());
                baseClip =
                        document->clipManager().insertTextClip(GenTime(clip.attribute("duration", "").toInt(), KdenliveSettings::defaultfps()), clip.attribute("name", ""),
                clip.attribute("description", ""), xml, clip.attribute("resource", ""), pm, clip.attribute("transparency", "").toInt(), clip.attribute("id", "-1").toInt());
            }
            else if (clipType == DocClipBase::VIRTUAL)	//   VIRTUAL CLIP
            {
                baseClip =
                        document->clipManager().insertVirtualClip(clip.attribute("name", ""), clip.attribute("description", ""), GenTime(clip.attribute("virtualstart", "").toInt(), KdenliveSettings::defaultfps()), GenTime(clip.attribute("virtualend", "").toInt(), KdenliveSettings::defaultfps()), clip.attribute("resource", ""), clip.attribute("frame_thumbnail", "0").toInt(), clip.attribute("id", "-1").toInt());
            }

	    if (baseClip) {
		QDomNode n = clip.firstChild();
    		while (!n.isNull()) {
		    QDomElement e = n.toElement();
		    if (!e.isNull()) {
		    	if (e.tagName() == "markers") {
			    QDomNode markerNode = e.firstChild();
			    while (!markerNode.isNull()) {
		    	        QDomElement markerElement = markerNode.toElement();
		    	        if (!markerElement.isNull()) {
			 	    if (markerElement.tagName() == "marker") {
			    	    	markers.append(CommentedTime(GenTime(markerElement.attribute("time","0").toDouble()),markerElement.attribute("comment", "")));
				    } else {
			    	    	kdWarning() << "Unknown tag " << markerElement.tagName() << endl;
				    }
		                }
		    	        markerNode = markerNode.nextSibling();
			    }
	    	    	}
		    }
		    n = n.nextSibling();
	    	}

		baseClip->setSnapMarkers(markers);
	    	DocumentClipNode *clipNode = new DocumentClipNode(parentNode, baseClip);
	    	thisNode = clipNode;
		
	    	QString desc;
	    	// find description, if one exists.
	    	n = clip.firstChild();
	    	while (!n.isNull()) {
			QDomText t = n.toText();
			if (!t.isNull()) {
		    		desc.append(t.nodeValue());
			}

			n = n.nextSibling();
	    	}

	    	clipNode->clipRef()->setDescription(desc);
	    }
	    else {
		// clip does not exist
		kdDebug()<<"+++ One clip removed from list"<<endl;
		thisNode = 0;
	    }
	}
	else if (clip.tagName() == "folder") {
		thisNode = new DocumentGroupNode(0, clip.attribute("name", ""));
	}
    } else {
	kdError() << "Could not find document base node " << parent << " in document" << endl;
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
