/***************************************************************************
                          kmmtrackvideopanel.h  -  description
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

#ifndef KMMTRACKVIDEOPANEL_H
#define KMMTRACKVIDEOPANEL_H

#include <qwidget.h>
#include <qlabel.h>
#include <qhbox.h>
#include <qpixmap.h>

#include "trackheader_ui.h"
#include "doctrackvideo.h"
#include "kdenlive.h"
#include "kmmtrackpanel.h"

class KTimeLine;

/**KMMTrackVideoPanel contains useful controls for manipulating the video tracks
which reside in the main video widget
  *@author Jason Wood
  */

class KdenliveDoc;

namespace Gui {

    class KMMTrackVideoPanel:public KMMTrackPanel {
      Q_OBJECT public:
	KMMTrackVideoPanel(KdenliveApp * app,
	    KTimeLine * timeline,
	    KdenliveDoc * document,
	    DocTrackVideo * docTrack,
	    bool isCollapsed, QWidget * parent = 0, const char *name = 0);
	~KMMTrackVideoPanel();

	private slots:void resizeTrack();
	void decorateTrack();

      private:
	 QHBox m_horzLayout;
	TrackHeader m_trackHeader;
	/** True if we are inside a dragging operation, false otherwise. */
	bool m_dragging;

	 signals: void collapseTrack(KTrackPanel *, bool);
    };

}				// namespace Gui
#endif
