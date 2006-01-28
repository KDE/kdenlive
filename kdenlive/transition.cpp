/***************************************************************************
                          transition.cpp  -  description
                             -------------------
    begin                : Tue Jan 24 2006
    copyright            : (C) 2006 by Jean-Baptiste Mardelle
    email                : jb@ader.ch 
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "transition.h"

#include <kdebug.h>
#include <qdom.h>


Transition::Transition(const DocClipRef * clipa, const DocClipRef * clipb)
{
    if (clipa->trackStart() < clipb->trackStart()) {
	m_clipa = clipa;
	m_clipb = clipb;
    } else {
	m_clipa = clipb;
	m_clipb = clipa;
    }
}

Transition::~Transition()
{
}

uint Transition::transitionStartTrack()
{
    return m_clipa->trackNum();
}

uint Transition::transitionEndTrack()
{
    return m_clipb->trackNum();
}

double Transition::transitionStartTime()
{
    double startb = m_clipb->trackStart().frames(25);
    double starta = m_clipa->trackStart().frames(25);
    if (startb > starta)
	return startb;
    return starta;
}


double Transition::transitionEndTime()
{
    double startb = m_clipb->trackStart().frames(25);
    double enda =
	m_clipa->trackStart().frames(25) +
	m_clipa->cropDuration().frames(25);
    if (startb > enda)
	return transitionStartTime();
    return enda;
}


Transition *Transition::hasClip(const DocClipRef * clip)
{
    if (clip == m_clipa || clip == m_clipb)
	return this;
    return NULL;
}

Transition *Transition::clone()
{
    return new Transition::Transition(m_clipa, m_clipb);
}
