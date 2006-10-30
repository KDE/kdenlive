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
#include "kmmclipkeyframepanel.h"

#include "kclipplacer.h"

#include "trackviewbackgrounddecorator.h"
#include "trackviewdoublekeyframedecorator.h"

namespace Gui {

    KMMClipKeyFramePanel::KMMClipKeyFramePanel(KTimeLine * timeline,
	KdenliveDoc * document,
	DocClipRef * clip,
	const QString & effectName,
	int effectIndex,
	const QString & effectParam, QWidget * parent, const char *name)
    :KMMTrackPanel(timeline, document, new KClipPlacer(timeline, clip),
	EFFECTKEYFRAMETRACK, parent, name),
	m_verticalLayout(this, "KMMClipKeyFramePanel::verticalLayout"),
	m_effectNameLabel(effectName, &m_verticalLayout,
	"KMMClipKeyFramePanel::effectNameLabel") {
	setMinimumHeight(50);
	setMaximumHeight(50);

	// addFunctionDecorator("move", "resize");

	addViewDecorator(new TrackViewBackgroundDecorator(timeline,
		document, QColor(200, 200, 200)));
	addViewDecorator(new TrackViewDoubleKeyFrameDecorator(timeline,
		document));
    } 

    KMMClipKeyFramePanel::~KMMClipKeyFramePanel() {
    }

    //virtual
    void KMMClipKeyFramePanel::setSelected(bool isSelected)
    {
    }

}				// namespace Gui
