/***************************************************************************
                          loadprojectkinofilter  -  description
                             -------------------
    begin                : Sun Dec 14 2003
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
#include "loadprojectkinofilter.h"

#include <qdom.h>
#include <qfile.h>

#include <kdebug.h>

#include "documentclipnode.h"
#include "docclipproject.h"
#include "docclipref.h"
#include "doctrackvideo.h"
#include "gentime.h"
#include "kdenlivedoc.h"

LoadProjectKinoFilter::LoadProjectKinoFilter()
:  LoadProjectFilter()
{
}


LoadProjectKinoFilter::~LoadProjectKinoFilter()
{
}


bool LoadProjectKinoFilter::load(QFile & file, KdenliveDoc * document)
{
    bool success = false;
    QDomDocument doc;
    doc.setContent(&file, false);

    QDomElement documentElement = doc.documentElement();

    DocClipProject *project = new DocClipProject(document->projectClip().framesPerSecond(), document->projectClip().videoWidth(), document->projectClip().videoHeight());
    project->slotAddTrack(new DocTrackVideo(project));

    GenTime currentTrackTime = GenTime(0);

    if (documentElement.tagName() == "smil") {
	success = true;

	QDomNode n = documentElement.firstChild();

	while (!n.isNull()) {
	    QDomElement e = n.toElement();
	    if (!e.isNull()) {
		if (e.tagName() == "seq") {
		    currentTrackTime =
			loadSeq(currentTrackTime, e, project->track(0),
			document);
		} else {
		    kdWarning() << "Loading smil : unknown element " << e.
			tagName() << ", ignoring..." << endl;
		}
	    }

	    n = n.nextSibling();
	}
    }

    document->setProjectClip(project);

    return success;
}

GenTime LoadProjectKinoFilter::loadSeq(const GenTime & currentTrackTime,
    const QDomElement & seqElem, DocTrackBase * track,
    KdenliveDoc * document)
{
    GenTime newTrackTime = currentTrackTime;

    QDomNode n = seqElem.firstChild();
    while (!n.isNull()) {
	QDomElement e = n.toElement();
	if (!e.isNull()) {
	    if (e.tagName() == "video") {
		DocClipBase *file =
		    document->clipManager().insertClip(KURL(e.
			attribute("src", "")));
		DocumentClipNode *clipNode =
		    new DocumentClipNode(document->clipHierarch(), file);
		document->addClipNode(document->clipHierarch()->name(),
		    clipNode);

		DocClipRef *ref = new DocClipRef(file);

		GenTime cropStart =
				GenTime(e.attribute("clipBegin", "0").toInt(),///was toDouble
		    track->framesPerSecond());
		GenTime cropDuration =
				GenTime(e.attribute("clipEnd", "0").toInt(),///was toDouble
		    track->framesPerSecond()) - cropStart;;

		ref->setTrackStart(newTrackTime);
		ref->setCropStartTime(cropStart);
		ref->setCropDuration(cropDuration);

		track->addClip(ref, false);

		newTrackTime += cropDuration;
	    } else {
		kdWarning() << "Loading smil : unknown element " << e.
		    tagName() << ", ignoring..." << endl;
	    }
	}

	n = n.nextSibling();
    }

    return newTrackTime;
}

QStringList LoadProjectKinoFilter::handledFormats() const
{
    QStringList list;

    list.append("application/smil");

    return list;
}
