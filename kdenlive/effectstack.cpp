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
#include "effectstack.h"

#include <kdebug.h>

EffectStack::EffectStack()
:  QPtrList < Effect > (), index(0)
{
    setAutoDelete(true);
}

EffectStack::EffectStack(const EffectStack & rhs):QPtrList < Effect > ()
{
    for (QPtrListIterator < Effect > itt(rhs); itt.current(); ++itt) {
	Effect *effect = itt.current()->clone();

	if (effect) {
	    append(effect);
	} else {
	    kdError() << "EffectStack copy constructor failed. " << endl;
	}
    }
}

const EffectStack & EffectStack::operator=(const EffectStack & rhs)
{
    clear();
    for (QPtrListIterator < Effect > itt(rhs); itt.current(); ++itt) {
	Effect *effect = itt.current()->clone();

	if (effect) {
	    append(effect);
	} else {
	    kdError() << "EffectStack copy constructor failed. " << endl;
	}
    }

    return *this;
}

EffectStack::~EffectStack()
{
}

void EffectStack::setSelected(uint ix)
{
    index = ix;
}

Effect *EffectStack::selectedItem()
{
    return at(index);
}

const uint EffectStack::selectedItemIndex() const
{
    uint max = count();
    if (max == 0) return 0;
    if (index + 1 > max) return max -1;
    return index;
}

Effect *EffectStack::operator[] (int ix)
const {
    EffectStackIterator itt(*this);
    int count = 0;
    while (itt.current() && (count != ix))
{
++itt;
++count;
} return itt.current();
}
