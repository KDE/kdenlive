/***************************************************************************
                          trackviewtransitiondecorator  -  description
                             -------------------
    begin                : Thu Jan 26 2006
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
#ifndef TRACKVIEWTRANSITIONDECORATOR_H
#define TRACKVIEWTRANSITIONDECORATOR_H

#include <qstring.h>

#include <doctrackdecorator.h>
#include "transition.h"

namespace Gui {
    class KTimeLine;

/**
@author Jason Wood
*/
    class TrackViewTransitionDecorator:public DocTrackDecorator {
      public:
	TrackViewTransitionDecorator(KTimeLine * timeline, KdenliveDoc * doc);

	~TrackViewTransitionDecorator();

	virtual void paintClip(double startX, double endX,
	    QPainter & painter, DocClipRef * clip, QRect & rect,
	    bool selected);
      private:
	QString m_effectName;
	int m_effectIndex;
	QString m_paramName;
    };

}				// namespace Gui
#endif
