/***************************************************************************
                          saveprojectnativefilter  -  description
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
#include "saveprojectnativefilter.h"

#include <qfile.h>
#include <qstring.h>

#include <kdebug.h>

#include "kdenlivedoc.h"

#include "documentgroupnode.h"
#include "docclipproject.h"
#include "docclipavfile.h"
#include "doccliptextfile.h"

SaveProjectNativeFilter::SaveProjectNativeFilter()
:  SaveProjectFilter()
{
}


SaveProjectNativeFilter::~SaveProjectNativeFilter()
{
}

// virtual
bool SaveProjectNativeFilter::save(QFile & file, KdenliveDoc * document)
{
    QDomDocument doc;

    QDomElement elem = doc.createElement("kdenlivedoc");
    doc.appendChild(elem);
    QDomElement docinfos = doc.createElement("properties");
    docinfos.setAttribute("projectfolder", KdenliveSettings::currentdefaultfolder());
    docinfos.setAttribute("projectheight", QString::number(KdenliveSettings::defaultheight()));
    docinfos.setAttribute("projectwidth", QString::number(KdenliveSettings::defaultwidth()));
    docinfos.setAttribute("timeline_position", document->renderer()->seekPosition().frames(KdenliveSettings::defaultfps()));
    docinfos.setAttribute("projectfps", QString::number(KdenliveSettings::defaultfps()));
    docinfos.setAttribute("projectratio", QString::number(KdenliveSettings::aspectratio()));
    docinfos.setAttribute("projectguides", document->guidesStringList());
    docinfos.setAttribute("projectguidescomments", document->application()->timelineGuidesComments());

    docinfos.setAttribute("inpoint",  QString::number(document->application()->inpointPosition().frames(KdenliveSettings::defaultfps())));
    docinfos.setAttribute("outpoint", QString::number(document->application()->outpointPosition().frames(KdenliveSettings::defaultfps())));

    elem.appendChild(docinfos);

    QDomElement avfilelist = doc.createElement("avfilelist");
    DocumentBaseNode *node = document->clipHierarch();
    QPtrListIterator < DocumentBaseNode > itt(node->children());

    while (itt.current()) {
	DocumentClipNode *clipNode = itt.current()->asClipNode();
	if (clipNode) {
	    QDomElement avfile = doc.createElement("avfile");
	    if (clipNode->clipRef()->name() != "")
		avfilelist.appendChild(processedNode(clipNode, avfile));
	} else {
	    QDomElement folderItem = doc.createElement("folder");
	    folderItem.setAttribute("name", itt.current()->name());
	    DocumentGroupNode *folder = document->findClipNode(itt.current()->name())->asGroupNode();
    	    QPtrListIterator < DocumentBaseNode > itt(folder->children());

    	    while (itt.current()) {
		DocumentClipNode *clipNode = itt.current()->asClipNode();
		if (clipNode) {
			QDomElement avfile = doc.createElement("avfile");
	        	if (clipNode->clipRef()->name() != "")
			folderItem.appendChild(processedNode(clipNode, avfile));
		} 
		++itt;
        }

	avfilelist.appendChild(folderItem);

	    //kdError() << "no support for saving group nodes yet!" << endl;
	}
	++itt;
    }


    elem.appendChild(avfilelist);

    elem.appendChild(doc.importNode(document->projectClip().toXML().documentElement(), true));

    QString save = doc.toString();
    file.writeBlock(save.utf8(), save.length());

    return true;
}

QDomElement SaveProjectNativeFilter::processedNode(DocumentClipNode *clipNode, QDomElement avfile)
{
	avfile.setAttribute("url", clipNode->clipRef()->fileURL().path());
	DocClipBase::CLIPTYPE clipType;
	clipType = clipNode->clipRef()->clipType();
	avfile.setAttribute("type", clipType);
            
            avfile.setAttribute("id", clipNode->clipRef()->referencedClip()->getId());

	    if (clipType == DocClipBase::IMAGE) {
		avfile.setAttribute("duration",
		    QString::number(clipNode->clipRef()->duration().
			frames(KdenliveSettings::defaultfps())));
                avfile.setAttribute("transparency",clipNode->clipRef()->referencedClip()->toDocClipAVFile()->isTransparent());
            }
	    else if (clipType == DocClipBase::SLIDESHOW) {
		avfile.setAttribute("duration",
		    QString::number(clipNode->clipRef()->duration().
			frames(KdenliveSettings::defaultfps())));
		avfile.setAttribute("ttl",
		    QString::number(clipNode->clipRef()->referencedClip()->toDocClipAVFile()->clipTtl()));
                avfile.setAttribute("transparency",clipNode->clipRef()->referencedClip()->toDocClipAVFile()->isTransparent());
		avfile.setAttribute("crossfade",clipNode->clipRef()->referencedClip()->toDocClipAVFile()->hasCrossfade());
            }
            else if (clipType == DocClipBase::COLOR) {
		avfile.setAttribute("name", clipNode->clipRef()->name());
		avfile.setAttribute("color",
		    clipNode->clipRef()->referencedClip()->
		    toDocClipAVFile()->color());
		avfile.setAttribute("duration",
		    QString::number(clipNode->clipRef()->duration().
			frames(KdenliveSettings::defaultfps())));
	    }
            else if (clipType == DocClipBase::TEXT) {
                avfile.setAttribute("duration",
                                    QString::number(clipNode->clipRef()->duration().
                                            frames(KdenliveSettings::defaultfps())));
                avfile.setAttribute("name", clipNode->clipRef()->name());
                avfile.setAttribute("transparency",clipNode->clipRef()->referencedClip()->toDocClipTextFile()->isTransparent());
                avfile.setAttribute("xml",
                                    clipNode->clipRef()->referencedClip()->toDocClipTextFile()->textClipXml().toString());
            }
	    QDomText description = avfile.ownerDocument().createTextNode(clipNode->clipRef()->description());
	    avfile.appendChild(description);

	    return avfile;
}

// virtual
QStringList SaveProjectNativeFilter::handledFormats() const
{
    QStringList list;

    list.append("application/vnd.kde.kdenlive");

    return list;
}
