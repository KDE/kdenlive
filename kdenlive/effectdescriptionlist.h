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
#ifndef EFFECTDESCRIPTIONLIST_H
#define EFFECTDESCRIPTIONLIST_H

#include <qptrlist.h>

class EffectDesc;
class QString;

/**
A list of effect descriptions.

@author Jason Wood
*/
class EffectDescriptionList:public QPtrList < EffectDesc > {
  public:
    EffectDescriptionList();

    ~EffectDescriptionList();

    EffectDesc *effectDescription(const QString & type) const;
    const QString stringId(QString effectName) const;
};

#endif
