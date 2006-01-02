/***************************************************************************
                          kmmtrackkeyframepanel.h  -  description
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

#ifndef KMMTRACKKEYFRAMEPANEL_H
#define KMMTRACKKEYFRAMEPANEL_H

#include <qwidget.h>
#include <kmmtrackpanel.h>

#include "doctrackbase.h"

class KdenliveDoc;

namespace Gui
{

class KTimeLine;

/**This panel widget handles the manipulation of keyframes.
  *@author Jason Wood
  */

class KMMTrackKeyFramePanel : public KMMTrackPanel  {
   Q_OBJECT
public:
	KMMTrackKeyFramePanel(KTimeLine *timeline,
				KdenliveDoc *document,
				DocTrackBase *docTrack,
				bool isCollapsed, 
				const QString &effectName,
				int effectIndex,
				const QString &effectParam,
				TRACKTYPE type,
				QWidget *parent=0,
				const char *name=0);
	virtual ~KMMTrackKeyFramePanel();

public slots:
	void resizeTrack();
private:
	TRACKTYPE m_type;
signals:
	void collapseTrack(KTrackPanel *, bool);
};

} // namespace Gui
#endif
