/***************************************************************************
                          docclipbase.cpp  -  description
                             -------------------
    begin                : Fri Apr 12 2002
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

#include "docclipbase.h"

DocClipBase::DocClipBase()
{
}

DocClipBase::~DocClipBase()
{
}

/** Returns where the start of this clip is on the track is resides on. */
long DocClipBase::trackStart(){
	return m_trackStart;
}

