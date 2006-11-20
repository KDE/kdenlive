/***************************************************************************
                          effectdescriptionlist  -  description
                             -------------------
    begin                : Tue Feb 10 2004
    copyright            : (C) 2004 by Jason Wood
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
#include "effectdescriptionlist.h"
#include "effectparamdesc.h"
#include "effectdesc.h"

EffectDescriptionList::EffectDescriptionList()
:  QPtrList < EffectDesc > ()
{
}


EffectDescriptionList::~EffectDescriptionList()
{
}

EffectDesc *EffectDescriptionList::effectDescription(const QString & type) const
{
    EffectDesc *result = 0;
    QPtrListIterator < EffectDesc > itt(*this);

    while (itt.current()) {
	if (itt.current()->name() == type) {
	    //result = itt.current();
	    break;
	}
	++itt;
    }

    if (itt.current()) {
	result = new EffectDesc( itt.current()->name(), itt.current()->tag(),itt.current()->type(), itt.current()->isMono());
	   for (uint count = 0; count < itt.current()->numParameters(); ++count)
		result->addParameter(itt.current()->parameter(count)->clone());
    }
    return result;
}
