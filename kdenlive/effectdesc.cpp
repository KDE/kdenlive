/***************************************************************************
                          effectdesc.cpp  -  description
                             -------------------
    begin                : Sun Feb 9 2003
    copyright            : (C) 2003 by Jason Wood
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

#include "effectdesc.h"

EffectDesc::EffectDesc(const QString &name) :
                          m_name(name)
{
}

EffectDesc::~EffectDesc()
{
}

/** Returns the name of this effect. */
QString EffectDesc::name()
{
  return m_name;
}
