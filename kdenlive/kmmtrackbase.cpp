/***************************************************************************
                          kmmtrackbase.cpp  -  description
                             -------------------
    begin                : Sat Feb 16 2002
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

#include "kmmtrackbase.h"
#include <qpainter.h>
#include <qsizepolicy.h>

KMMTrackBase::KMMTrackBase(QWidget *parent, const char *name ) : QFrame(parent,name)
{	
	setMinimumWidth(30);
	setMinimumHeight(30);	
	
	setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Minimum));	
	setPalette( QPalette( QColor(170, 170, 170) ) );
	
	setFrameStyle(QFrame::Panel | QFrame::Sunken);
}

KMMTrackBase::~KMMTrackBase() {
}