/***************************************************************************
                          kmmtimeline.cpp  -  description
                             -------------------
    begin                : Fri Feb 15 2002
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

#include "kmmtimeline.h"

KMMTimeLine::KMMTimeLine(QWidget *parent, const char *name ) : QVBox(parent, name),
				rulerBox(this, "ruler box"),
				trackView(this, "track view", 0),
				trackBox(&trackView, "track box"),
				scrollBox(this, "scroll box"),
				trackLabel("tracks", &rulerBox),
				ruler(&rulerBox, name),
				scrollLabel("tracks", &scrollBox),
				scrollBar(0, 5000, 50, 500, 0, QScrollBar::Horizontal, &scrollBox, "horizontal ScrollBar")
{
	trackLabel.setMinimumWidth(200);
	trackLabel.setMaximumWidth(200);
	trackLabel.setAlignment(AlignCenter);
	
	scrollLabel.setMinimumWidth(200);
	scrollLabel.setMaximumWidth(200);
	scrollLabel.setAlignment(AlignCenter);	
	
	connect(&scrollBar, SIGNAL(valueChanged(int)), &ruler, SLOT(setValue(int)));
}

KMMTimeLine::~KMMTimeLine()
{
}

