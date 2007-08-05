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
#include "docclipvirtual.h"
#include "kdenlive.h"

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

    QDomElement westley = doc.createElement("westley");
    doc.appendChild(westley);
    QDomElement elem = doc.createElement("kdenlivedoc");
    elem.setAttribute("version", "0.6");
    westley.appendChild(elem);
    QDomElement docinfos = doc.createElement("properties");
    docinfos.setAttribute("projectfolder", KdenliveSettings::currentdefaultfolder());
    docinfos.setAttribute("projectheight", QString::number(KdenliveSettings::defaultheight()));
    docinfos.setAttribute("projectwidth", QString::number(KdenliveSettings::defaultwidth()));
    docinfos.setAttribute("timeline_position", document->renderer()->seekPosition().frames(KdenliveSettings::defaultfps()));
    docinfos.setAttribute("projectfps", QString::number(KdenliveSettings::defaultfps()));
    docinfos.setAttribute("projectratio", QString::number(KdenliveSettings::aspectratio()));
    docinfos.setAttribute("projectdisplayratio", QString::number(KdenliveSettings::displayratio()));
    docinfos.setAttribute("projectvideoformat", document->application()->projectVideoFormat());
    docinfos.setAttribute("videoprofile", KdenliveSettings::videoprofile());
    docinfos.setAttribute("metadata", document->metadata().join("#"));

    docinfos.setAttribute("inpoint",  QString::number(document->application()->inpointPosition().frames(KdenliveSettings::defaultfps())));
    docinfos.setAttribute("outpoint", QString::number(document->application()->outpointPosition().frames(KdenliveSettings::defaultfps())));

    elem.appendChild(docinfos);
    elem.appendChild(doc.importNode(document->application()->xmlGuides().documentElement(), true));

    QDomDocumentFragment avfilelist = doc.createDocumentFragment();
    DocumentBaseNode *node = document->clipHierarch();
    QPtrListIterator < DocumentBaseNode > itt(node->children());

    // Black producer for background color
    QDomElement black = doc.createElement("producer");
    black.setAttribute("id", "black");
    black.setAttribute("mlt_service", "colour");
    black.setAttribute("colour", "black");
    avfilelist.appendChild(black);

    while (itt.current()) {
	DocumentClipNode *clipNode = itt.current()->asClipNode();
	if (clipNode) {
	    QDomElement avfile = doc.createElement("producer");
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
			QDomElement avfile = doc.createElement("producer");
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


    // Append Kdenlive's track list
    QDomDocument trackList = document->projectClip().toXML();
    if (!trackList.isNull()) elem.appendChild(doc.importNode(trackList.documentElement(), true));

    // include a copy of the MLT playlist so that the project file can be played directly
    QDomDocument westleyList = document->projectClip().generateSceneList(false);
    if (!westleyList.isNull()) {
	QDomNode playlist = doc.importNode(westleyList.documentElement(), true);
    	westley.appendChild(playlist);
    }

    QCString save = doc.toString().utf8();
    file.writeBlock(save, save.length());

    return true;
}

QDomElement SaveProjectNativeFilter::processedNode(DocumentClipNode *clipNode, QDomElement avfile)
{

	DocClipBase::CLIPTYPE clipType;
	clipType = clipNode->clipRef()->clipType();
	avfile.setAttribute("type", clipType);
        avfile.setAttribute("id", clipNode->clipRef()->referencedClip()->getId());
	if (clipType != DocClipBase::COLOR) 
	    avfile.setAttribute("resource", clipNode->clipRef()->fileURL().path());

	    if (clipType == DocClipBase::IMAGE) {
		avfile.setAttribute("duration",
		    QString::number(clipNode->clipRef()->duration().
			frames(KdenliveSettings::defaultfps())));
		avfile.setAttribute("hide", "audio");
		// avfile.setAttribute("aspect_ratio", QString::number(clipNode->clipRef()->referencedClip()->toDocClipAVFile()->aspectRatio()));
                avfile.setAttribute("transparency",clipNode->clipRef()->referencedClip()->toDocClipAVFile()->isTransparent());
            }
	    else if (clipType == DocClipBase::SLIDESHOW) {
		avfile.setAttribute("duration",
		    QString::number(clipNode->clipRef()->duration().
			frames(KdenliveSettings::defaultfps())));
		avfile.setAttribute("ttl",
		    QString::number(clipNode->clipRef()->referencedClip()->toDocClipAVFile()->clipTtl()));
		avfile.setAttribute("lumafile", clipNode->clipRef()->referencedClip()->toDocClipAVFile()->lumaFile());
		avfile.setAttribute("lumasoftness", QString::number(clipNode->clipRef()->referencedClip()->toDocClipAVFile()->lumaSoftness()));
		avfile.setAttribute("lumaduration", QString::number(clipNode->clipRef()->referencedClip()->toDocClipAVFile()->lumaDuration()));
                avfile.setAttribute("transparency",clipNode->clipRef()->referencedClip()->toDocClipAVFile()->isTransparent());
		avfile.setAttribute("hide", "audio");
		avfile.setAttribute("aspect_ratio", QString::number(clipNode->clipRef()->referencedClip()->toDocClipAVFile()->aspectRatio()));
		avfile.setAttribute("crossfade",clipNode->clipRef()->referencedClip()->toDocClipAVFile()->hasCrossfade());
            }
            else if (clipType == DocClipBase::COLOR) {
		avfile.setAttribute("name", clipNode->clipRef()->name());
		avfile.setAttribute("mlt_service", "colour");
		avfile.setAttribute("hide", "audio");
		double ratio = KdenliveSettings::aspectratio();
		ratio = ratio / ((double) KdenliveSettings::defaultwidth()/KdenliveSettings::defaultheight());
		//avfile.setAttribute("aspect_ratio", QString::number( ratio ));
		avfile.setAttribute("colour",
		    clipNode->clipRef()->referencedClip()->
		    toDocClipAVFile()->color());
		avfile.setAttribute("duration",
		    QString::number(clipNode->clipRef()->duration().
			frames(KdenliveSettings::defaultfps())));
	    }
            else if (clipType == DocClipBase::TEXT) {
		DocClipTextFile *tclip = clipNode->clipRef()->referencedClip()->toDocClipTextFile();
                avfile.setAttribute("duration",
                                    QString::number(clipNode->clipRef()->duration().
                                            frames(KdenliveSettings::defaultfps())));
                avfile.setAttribute("name", clipNode->clipRef()->name());
                avfile.setAttribute("transparency", tclip->isTransparent());
		avfile.setAttribute("hide", "audio");
		avfile.setAttribute("aspect_ratio", QString::number( tclip->aspectRatio()));
		QDomDocument clipText = tclip->textClipXml();
		avfile.appendChild(avfile.ownerDocument().importNode(clipText.documentElement(), true));
            }
            else if (clipType == DocClipBase::VIRTUAL) {
		DocClipVirtual *vclip = clipNode->clipRef()->referencedClip()->toDocClipVirtual();
                avfile.setAttribute("duration",
                                    QString::number(clipNode->clipRef()->duration().
                                            frames(KdenliveSettings::defaultfps())));
                avfile.setAttribute("name", clipNode->clipRef()->name());
		avfile.setAttribute("virtualstart", vclip->virtualStartTime().frames(KdenliveSettings::defaultfps()));
		avfile.setAttribute("virtualend", vclip->virtualEndTime().frames(KdenliveSettings::defaultfps()));
		avfile.setAttribute("frame_thumbnail", QString::number(clipNode->clipRef()->referencedClip()->getProjectThumbFrame()));
            }
	    else {
		avfile.setAttribute("frame_thumbnail", QString::number(clipNode->clipRef()->referencedClip()->getProjectThumbFrame()));
	    }

	    QString desc = clipNode->clipRef()->description();
	    if (!desc.isEmpty()) {
	    	QDomText description =  avfile.ownerDocument().createTextNode(desc);
	    	avfile.appendChild(description);
	    }

	    QValueVector < CommentedTime > originalMarkers = clipNode->clipRef()->commentedSnapMarkers();
    	    if (originalMarkers.count() > 0) {
        	QDomElement markers = avfile.ownerDocument().createElement("markers");
        	for (uint count = 0; count < originalMarkers.count(); ++count) {
	    	    QDomElement marker = avfile.ownerDocument().createElement("marker");
	    	    marker.setAttribute("time", QString::number(originalMarkers[count].time().seconds(), 'f', 10));
	    	    marker.setAttribute("comment", originalMarkers[count].comment());
	    	    markers.appendChild(marker);
    	        }
    	        avfile.appendChild(markers);
    	    }



	    return avfile;
}

// virtual
QStringList SaveProjectNativeFilter::handledFormats() const
{
    QStringList list;

    list.append("application/vnd.kde.kdenlive");

    return list;
}
