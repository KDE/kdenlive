/***************************************************************************
                         ktrackclippanel  -  description
                            -------------------
   begin                : Thu Jan 8 2004
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
#ifndef KTRACKCLIPPANEL_H
#define KTRACKCLIPPANEL_H

#include <ktrackpanel.h>

/**
A track panel that understands the concept of "clips" on a track.

@author Jason Wood
*/
class KTrackClipPanel : public KTrackPanel
{
	Q_OBJECT
public:
	KTrackClipPanel( KTimeLine *timeline, QWidget *parent = 0, const char *name = 0 );

	virtual ~KTrackClipPanel();

	/** Draw the area rect, using the provided painter. */
	virtual void drawToBackBuffer( QPainter &painter, QRect &rect );
};

#endif
