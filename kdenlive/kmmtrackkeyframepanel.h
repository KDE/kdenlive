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

class KdenliveApp;
class KdenliveDoc;
class KTimeLine;

/**This panel widget handles the manipulation of keyframes.
  *@author Jason Wood
  */

namespace Command {
	class KResizeCommand;
}

class KMMTrackKeyFramePanel : public KMMTrackPanel  {
   Q_OBJECT
public:
	enum ResizeState {None, Start, End};
	KMMTrackKeyFramePanel(KdenliveApp *app,
				KTimeLine *timeline,
				KdenliveDoc *document,
				DocTrackBase *docTrack,
				const QString &effectName,
				int effectIndex,
				const QString &effectParam,
				QWidget *parent=0,
				const char *name=0);
	~KMMTrackKeyFramePanel();
private:
	/** During a resize operation, holds the current resize state, as defined in the ResizeState enum. */
	ResizeState m_resizeState;
	/** The clip that is under the mouse at present */
	DocClipBase * m_clipUnderMouse;
	/** True if we are inside a dragging operation, false otherwise. */
	bool m_dragging;
	/** This command holds the resize information during a resize operation */
	Command::KResizeCommand * m_resizeCommand;
};

#endif
