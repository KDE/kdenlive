/***************************************************************************
                         kmmtimeline  -  description
                            -------------------
   begin                : Wed Dec 24 2003
   copyright            : (C) 2003 by Jason Wood
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
#ifndef KMMTIMELINE_H
#define KMMTIMELINE_H

#include <ktimeline.h>
#include "qstring.h"

class DocClipRef;
class KMMRulerPanel;

/**
Implementation-specific derivation of KTimeLine. Includes correct drag/drop support, among other things.

@author Jason Wood
*/
class KMMTimeLine : public KTimeLine
{
public:
	KMMTimeLine( QWidget *scrollToolWidget, QWidget *parent = 0, const char *name = 0 );

	~KMMTimeLine();

	void fitToWidth();
	//set the timescale combobox to last saved value -reh
	void setSliderIndex( int index );
	//Return the current timescale slider value
	int getTimeScaleSliderValue() const;
	QString getTimeScaleSliderText() const;
	
public slots:
	/** Invalidates the area of the back buffer used by this clip. */
	void invalidateClipBuffer( DocClipRef *clip );
signals:
	/** emitted when something of interest is happening over a clip on the timeline. */
	void lookingAtClip( DocClipRef *, const GenTime & );
private:
	// A tool that can be used to change the timescale of the timeline.
	KMMRulerPanel *m_rulerToolWidget;
};

#endif
