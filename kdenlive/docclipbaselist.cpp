/***************************************************************************
                          docclipbaseiterator.cpp  -  description
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

#include "docclipbaselist.h"

#include <kdebug.h>

DocClipBaseList::DocClipBaseList() :
							QPtrList<DocClipBase>()
{
}

DocClipBaseList::~DocClipBaseList()
{
}

/** Compares Clips based upon starting time. */
int DocClipBaseList::compareItems (QPtrCollection::Item i1, QPtrCollection::Item i2)
{
	DocClipBase *item1 = (DocClipBase *)i1;
	DocClipBase *item2 = (DocClipBase *)i2;
	
	double diff = item1->trackStart().seconds() - item2->trackStart().seconds();

	if(diff==0) return 0;
  return (diff > 0) ? 1 : -1;
}
