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
#ifndef EFFECTSTACK_H
#define EFFECTSTACK_H

#include <qptrlist.h>

#include "effect.h"

/**
an Effect stack implements a list of events that should be applied one after the other. Although it is called a stack, you can insert/remove
elements from any point on the stack. However, this will change the output of effects further up the stack.

@author Jason Wood
*/
class EffectStack : public QPtrList<Effect>
{
public:
    EffectStack();
    EffectStack(const EffectStack &rhs);
    const EffectStack &operator=(const EffectStack&rhs);

    ~EffectStack();
};

typedef QPtrListIterator<Effect> EffectStackIterator;

#endif
