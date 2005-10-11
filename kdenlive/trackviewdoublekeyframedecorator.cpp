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

namespace Gui
{

TrackViewDoubleKeyFrameDecorator::TrackViewDoubleKeyFrameDecorator(KTimeLine *timeline, KdenliveDoc *doc, const QString &effectName, int effectIndex, const QString &paramName)
 							: DocTrackDecorator(timeline, doc),
							m_effectName(effectName),
							m_effectIndex(effectIndex),
							m_paramName(paramName)
{
}


TrackViewDoubleKeyFrameDecorator::~TrackViewDoubleKeyFrameDecorator()
{
}

// virtual
void TrackViewDoubleKeyFrameDecorator::paintClip(double startX, double endx, QPainter &painter, DocClipRef *clip, QRect &rect, bool selected)
{
}

} // namespace Gui