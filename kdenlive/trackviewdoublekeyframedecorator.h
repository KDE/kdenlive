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
#ifndef TRACKVIEWDOUBLEKEYFRAMEDECORATOR_H
#define TRACKVIEWDOUBLEKEYFRAMEDECORATOR_H

#include <qstring.h>

#include <doctrackdecorator.h>
#include "effect.h"

namespace Gui {
    class KTimeLine;

/**
@author Jason Wood
*/
    class TrackViewDoubleKeyFrameDecorator:public DocTrackDecorator {
      public:
	TrackViewDoubleKeyFrameDecorator(KTimeLine * timeline,
	    KdenliveDoc * doc, const QString & effectName, int effectIndex,
	    const QString & paramName);

	~TrackViewDoubleKeyFrameDecorator();

	virtual void paintClip(double startX, double endX,
	    QPainter & painter, DocClipRef * clip, QRect & rect,
	    bool selected);
      private:
	 QString m_effectName;
	int m_effectIndex;
	QString m_paramName;
	Effect *m_effect;
    };

}				// namespace Gui
#endif
