/***************************************************************************
                          effectkeyframe  -  description
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
#include "effectkeyframe.h"

EffectKeyFrame::EffectKeyFrame() :
				m_time(0)
{
}

EffectKeyFrame::EffectKeyFrame(double time) :
				m_time(time)
{
}

EffectKeyFrame::~EffectKeyFrame()
{
}


