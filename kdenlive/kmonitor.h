/***************************************************************************
                          kmonitor  -  description
                             -------------------
    begin                : Sun Jun 12 2005
    copyright            : (C) 2005 by Jason Wood
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
#ifndef GUIKMONITOR_H
#define GUIKMONITOR_H

#include <qvbox.h>
#include <kurl.h>

class DocClipRef;

namespace Gui {

    class KMMEditPanel;
    class KMMScreen;

/**
ABC class for the various monitor types that exist in Kdenlive

@author Jason Wood
*/
    class KMonitor:public QVBox {
      Q_OBJECT public:
	KMonitor(QWidget * parent = 0, const char *name = 0);

	~KMonitor();

        virtual void exportCurrentFrame(KURL url, bool notify) const;
	virtual KMMEditPanel *editPanel() const = 0;
	virtual KMMScreen *screen() const = 0;
	virtual DocClipRef *clip() const = 0;

	public slots:
	/** Sets this monitor to be the active monitor. It's colour changes to show it is active. */
	 virtual void slotSetActive() const;
	/** Sets this monitor to be an inactive monitor. It's colour changes to show it is inactive. */
	virtual void slotSetInactive() const;

	virtual void slotClearClip();

/** Toggles a snap marker on or off at the given position in the clip.. */
	virtual void slotToggleSnapMarker();

	 signals:
	/** Emitted when the mouse is clicked over the window. */
	void monitorClicked(KMonitor *);

    };

}
#endif
