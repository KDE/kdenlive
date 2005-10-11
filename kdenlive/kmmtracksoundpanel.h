/***************************************************************************
                          kmmtracksoundpanel.h  -  description
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

#ifndef KMMTRACKSOUNDPANEL_H
#define KMMTRACKSOUNDPANEL_H

#include <qwidget.h>
#include <qlabel.h>

#include "doctracksound.h"
#include "kmmtrackpanel.h"

class KdenliveDoc;

namespace Gui
{
	class KdenliveApp;
	class KTimeLine;

/**KMMTrackSoundPanel is the Track panel for sound files.
It contains several functions that can be used to manipulate
the main sound widget in different ways.
  *@author Jason Wood
  */

class KMMTrackSoundPanel : public KMMTrackPanel  {
   Q_OBJECT
public:
	KMMTrackSoundPanel(KdenliveApp *app,
				KTimeLine *timeline,
				KdenliveDoc *document,
				DocTrackSound *docTrack,
				QWidget *parent=0,
				const char *name=0);
	~KMMTrackSoundPanel();

private: // Public attributes
  /**  */
  QLabel m_trackLabel;
};

} // namespace Gui

#endif
