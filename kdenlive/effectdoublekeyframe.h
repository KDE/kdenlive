/***************************************************************************
                          effectdoublekeyframe  -  description
                             -------------------
    begin                : Fri Jan 2 2004
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
#ifndef EFFECTDOUBLEKEYFRAME_H
#define EFFECTDOUBLEKEYFRAME_H

#include <effectkeyframe.h>

/**
A keyframe value that holds a double.

@author Jason Wood
*/
class EffectDoubleKeyFrame : public EffectKeyFrame
{
public:
    EffectDoubleKeyFrame();

    ~EffectDoubleKeyFrame();

};

#endif
