/***************************************************************************
                          doctrackbaselist.h  -  description
                             -------------------
    begin                : Thu Nov 21 2002
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

#ifndef DOCTRACKBASELIST_H
#define DOCTRACKBASELIST_H

#include <qptrlist.h>

#include "doctrackbase.h"

class ClipManager;
class DocClipProject;

/**Contains a list of DocTrackBase objects.

Works the same as QPtrList<DocTrackBase>, but contains a number of extra functions such as toXML().
  *@author Jason Wood
  */

class DocTrackBaseList:public QPtrList < DocTrackBase > {
  public:
    DocTrackBaseList();
    DocTrackBaseList(const DocTrackBaseList & list);
    ~DocTrackBaseList();
	/** Returns an XML representation of this DocTrackBase list. */
    QDomDocument toXML();

	/** returns true if the contents of the xml element passed matches the contents of the track */
    bool matchesXML(const QDomElement & element) const;

	/** Generates the track list, based upon the XML list provided in elem. */
    void generateFromXML(const EffectDescriptionList & effectList,
	ClipManager & clipManager, DocClipProject * project,
	const QDomElement & elem);
};

typedef QPtrListIterator < DocTrackBase > DocTrackBaseListIterator;

#endif
