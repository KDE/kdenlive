/***************************************************************************
                          krulersliderbase.cpp  -  description
                             -------------------
    begin                : Sat Jul 20 2002
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

#include "krulersliderbase.h"

KRulerSliderBase::KRulerSliderBase()
{
	m_ref = 1;
}

KRulerSliderBase::~KRulerSliderBase()
{
}

KRulerSliderBase * KRulerSliderBase::newInstance()
{
	incrementRef();
	return this;	
}

/** Add one to the reference count. If this count reaches zero, the KRulerSliderBase is destroyed. */
void KRulerSliderBase::incrementRef()
{
	m_ref++;
}

/** Decreases reference count by one. If this count reaches zero, nothing is refering to the class and it can be deleted. */
void KRulerSliderBase::decrementRef(){
	m_ref--;
	if(!m_ref) delete this;
}
