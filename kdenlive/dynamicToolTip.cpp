/***************************************************************************
                          dynamicToolTip  -  description
                             -------------------
    begin                : Sun Nov 28 2004
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
#include "dynamicToolTip.h"
#include <qapplication.h>
#include <qpainter.h>
#include <stdlib.h>

#include "kmmtrackpanel.h"
#include "ktrackview.h"
#include "kdenlivedoc.h"

namespace Gui {

DynamicToolTip::DynamicToolTip(KTrackView * parent):QToolTip(parent), m_trackview(parent)
{
    // no explicit initialization needed
}


void DynamicToolTip::maybeTip(const QPoint & pos)
{
    KTrackPanel *panel = m_trackview->panelAt(pos.y());
    KMMTrackPanel *m_panel = static_cast<KMMTrackPanel*> (panel);
    DocClipRef *underMouse = m_panel->getClipAt(pos.x()); 
    if (!underMouse) return;

    QString messageTip;

	QRect r(m_panel->getLocalValue(underMouse->trackStart()), m_panel->y() - m_trackview->y(), abs(m_panel->getLocalValue(underMouse->duration()) - m_panel->getLocalValue(GenTime(0))), 20);
	messageTip = underMouse->description();
	if (messageTip.isEmpty()) messageTip = underMouse->name();
	tip(r, messageTip);

    QValueVector < CommentedTime > markers = underMouse->commentedTrackSnapMarkers();
    QValueVector < CommentedTime >::iterator itt = markers.begin();

    while (itt != markers.end()) {
	int x = m_panel->getLocalValue((*itt).time());
	if ( fabs(x - pos.x()) < 5) {
	    QRect r(x -7, m_panel->y() - m_trackview->y(), 15, m_panel->height());
	    tip(r, (*itt).comment());
	    break;
	}
	++itt;
    }

}

}				// namespace Gui
