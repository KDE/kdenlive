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
#ifndef TRANSITIONSTACK_H
#define TRANSITIONSTACK_H

#include <qptrlist.h>

#include "transition.h"

/**
an Effect stack implements a list of events that should be applied one after the other. Although it is called a stack, you can insert/remove
elements from any point on the stack. However, this will change the output of effects further up the stack.

The effects in the effect stack are applied in order with the first effect in the the list appied first and the last effect in the list applied last.

@author Jason Wood
*/
class TransitionStack:public QPtrList < Transition > {
  public:
    TransitionStack();

    Transition *selectedItem();
    void setSelected(uint ix);

    Transition *operator[] (int ix) const;

    ~TransitionStack();

  private:
     uint index;

};

typedef QPtrListIterator < Transition > TransitionStackIterator;

#endif
