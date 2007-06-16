/***************************************************************************
                          effectstack  -  description
                             -------------------
    begin                : Sat Jan 10 2004
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
#include "transitionstack.h"

#include <kdebug.h>

TransitionStack::TransitionStack()
:  QPtrList < Transition > (), index(0)
{
    setAutoDelete(true);
}

TransitionStack::~TransitionStack()
{
}

void TransitionStack::setSelected(uint ix)
{
    index = ix;
}

Transition* TransitionStack::exists(const DocClipRef * clipa, const DocClipRef * clipb)
{
    TransitionStackIterator itt(*this);
    while (itt.current()) {
        if ((*itt)->hasClip(clipa) && (*itt)->hasClip(clipb)) break;
        ++itt;
    }
    return itt.current();
}

Transition *TransitionStack::selectedItem()
{
    if (index + 1> count ()) index = count() -1;
    return at(index);
}

Transition *TransitionStack::operator[] (int ix) const {
    TransitionStackIterator itt(*this);
    int count = 0;
    while (itt.current() && (count != ix)) {
        ++itt;
        ++count;
    }
    return itt.current();
}
