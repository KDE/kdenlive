/***************************************************************************
                          kmmtrackvideopanel.cpp  -  description
                             -------------------
    begin                : Tue Apr 9 2002
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

#include "kmmtrackvideopanel.h"

KMMTrackVideoPanel::KMMTrackVideoPanel(DocTrackVideo *docTrack, QWidget *parent, const char *name ) :
												KMMTrackVideoPanel_UI(parent,name)
{
	setMinimumWidth(200);
	setMaximumWidth(200);
	setSizePolicy(QSizePolicy(QSizePolicy::Maximum, QSizePolicy::Expanding));
}

KMMTrackVideoPanel::~KMMTrackVideoPanel()
{
}
