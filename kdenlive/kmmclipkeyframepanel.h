/***************************************************************************
                         kmmclipkeyframepanel  -  description
                            -------------------
   begin                : Thu Apr 8 2004
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
#ifndef KMMCLIPKEYFRAMEPANEL_H
#define KMMCLIPKEYFRAMEPANEL_H

#include <kmmtrackpanel.h>

#include <qlabel.h>
#include <qvbox.h>

class KTimeLine;
class KdenliveDoc;
class DocClipRef;

namespace Gui {

/**
A track panel that displays the keyframe of a single parameter from a single clip,

@author Jason Wood
*/
    class KMMClipKeyFramePanel:public KMMTrackPanel {
      Q_OBJECT public:
	KMMClipKeyFramePanel(KTimeLine * timeline,
	    KdenliveDoc * document,
	    DocClipRef * clip,
	    const QString & effectName,
	    int effectIndex,
	    const QString & effectParam,
	    QWidget * parent = 0, const char *name = 0);
	    virtual void setSelected(bool isSelected);

	~KMMClipKeyFramePanel();
      private:
	 QVBox m_verticalLayout;
	QLabel m_effectNameLabel;
    };

}				// namespace Gui
#endif
