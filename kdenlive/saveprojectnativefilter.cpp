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
#include "documentclipnode.h"
#include "docclipproject.h"
#include "docclipavfile.h"

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

    QDomElement avfilelist = doc.createElement("avfilelist");
    DocumentBaseNode *node = document->clipHierarch();
    QPtrListIterator < DocumentBaseNode > itt(node->children());

    while (itt.current()) {
	DocumentClipNode *clipNode = itt.current()->asClipNode();

	if (clipNode) {
	    QDomElement avfile = doc.createElement("avfile");
	    avfile.setAttribute("url",
		clipNode->clipRef()->fileURL().path());
	    DocClipBase::CLIPTYPE clipType;
	    clipType = clipNode->clipRef()->clipType();
	    avfile.setAttribute("type", clipType);

	    if (clipType == DocClipBase::IMAGE)
		avfile.setAttribute("duration",
		    QString::number(clipNode->clipRef()->duration().
			frames(25)));

	    if (clipNode->clipRef()->clipType() == DocClipBase::COLOR) {
		avfile.setAttribute("name", clipNode->clipRef()->name());
		avfile.setAttribute("color",
		    clipNode->clipRef()->referencedClip()->
		    toDocClipAVFile()->color());
		avfile.setAttribute("duration",
		    QString::number(clipNode->clipRef()->duration().
			frames(25)));
	    }

	    QDomText description =
		doc.createTextNode(clipNode->clipRef()->description());
	    avfile.appendChild(description);

	    if (clipNode->clipRef()->name() != "")
		avfilelist.appendChild(avfile);
	} else {
	    kdError() << "no support for saving group nodes yet!" << endl;
	}
	++itt;
    }


    elem.appendChild(avfilelist);

    elem.appendChild(doc.importNode(document->projectClip().toXML().
	    documentElement(), true));

    QString save = doc.toString();
    file.writeBlock(save.ascii(), save.length());

    return true;
}

// virtual
QStringList SaveProjectNativeFilter::handledFormats() const
{
    QStringList list;

    list.append("application/vnd.kde.kdenlive");

    return list;
}
