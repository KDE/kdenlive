/***************************************************************************
                          trackviewdoublekeyframedecorator  -  description
                             -------------------
    begin                : Fri Jan 9 2004
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
#include "trackviewdoublekeyframedecorator.h"

TrackViewDoubleKeyFrameDecorator::TrackViewDoubleKeyFrameDecorator(KTimeLine *timeline, KdenliveDoc *doc, DocTrackBase *track, const QString &effectName, int effectIndex, const QString &paramName)
 							: DocTrackDecorator(timeline, doc, track),
							m_effectName(effectName),
							m_effectIndex(effectIndex),
							m_paramName(paramName)
{
}


TrackViewDoubleKeyFrameDecorator::~TrackViewDoubleKeyFrameDecorator()
{
}

void TrackViewDoubleKeyFrameDecorator::paintClip(QPainter& painter, DocClipRef* clip, QRect& rect, bool selected)
{
}
