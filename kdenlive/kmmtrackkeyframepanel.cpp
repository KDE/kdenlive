/***************************************************************************
                          kmmtrackkeyframepanel.cpp  -  description
                             -------------------
    begin                : Sun Dec 1 2002
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

#include "kmmtrackkeyframepanel.h"

KMMTrackKeyFramePanel::KMMTrackKeyFramePanel(KMMTimeLine &timeline, DocTrackBase &doc,
																													QWidget *parent, const char *name )
																				: KMMTrackPanel(timeline, doc, parent,name)
{
}

KMMTrackKeyFramePanel::~KMMTrackKeyFramePanel()
{
}

void KMMTrackKeyFramePanel::paintClip(QPainter & painter, DocClipBase * clip, QRect & rect, bool selected)
{
}
