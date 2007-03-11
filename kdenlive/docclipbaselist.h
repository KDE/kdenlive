/***************************************************************************
                          docclipbaseiterator.h  -  description
                             -------------------
    begin                : Sat Aug 10 2002
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

#ifndef DOCCLIPBASEITERATOR_H
#define DOCCLIPBASEITERATOR_H

#include <qptrlist.h>
#include "docclipbase.h"

/**An List for DocClipBase objects. Use this instead of QPtrList<DocClipBase> so as to sort lists correctly.
	* Also contains the ability to set a "master clip", which can be used by a number of operations where
	* the need for one clip to act as a reference for what happens to all clips is needed.
  * @author Jason Wood
  */
class ClipManager;
class KRender;

class DocClipBaseList:public QPtrList < DocClipBase > {
  public:
    DocClipBaseList();
    DocClipBaseList(const DocClipBaseList & list);
     DocClipBaseList & operator=(const DocClipBaseList & list);
    ~DocClipBaseList();
	/** Returns an XML version of this DocClipBaseList. The root element will take the name passed in.*/
    QDomDocument toXML(const QString & name);
	/** Sets the master clip to the one specified. If the clip is not in the clip list, then the
	method does not do anything. */
    void setMasterClip(DocClipBase * clip);
	/** Returns the current master clip for this cliplist. */
    DocClipBase *masterClip() const;

	/** Populates this clip list with the clips passed in via elem. */
    void generateFromXML(KRender * render, QDomElement elem);
  private:			// Private attributes
	/** The "master clip" of this list. The master clip is special only in that it is mentioned
	seperately to every other clip in the clip list; it also appears in the clip list. It is
	singled out so that operations that require a master clip can	make use of it. */
     DocClipBase * m_masterClip;
};

#endif
