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
  *@author Jason Wood
  */

class DocClipBaseList : public QPtrList<DocClipBase>  {
public: 
	DocClipBaseList();
	~DocClipBaseList();
  /** Compares Clips based upon starting time. */
	int DocClipBaseList::compareItems (QPtrCollection::Item i1, QPtrCollection::Item i2);
};

#endif
