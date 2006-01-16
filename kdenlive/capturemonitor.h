/***************************************************************************
                          capturemonitor  -  description
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
#ifndef GUICAPTUREMONITOR_H
#define GUICAPTUREMONITOR_H

#include <kmonitor.h>

class DocClipRef;
class KURLRequester;
class KListView;
class KLed;
class QToolButton;

namespace Gui {

    class KdenliveApp;
    class KMMEditPanel;
    class KMMScreen;

/**
A capture monitor, that allows you to capture video from various devices, as supported by the renderer.

@author Jason Wood
*/
    class CaptureMonitor:public KMonitor {
      Q_OBJECT public:
	CaptureMonitor(KdenliveApp * app, QWidget * parent =
	    0, const char *name = 0);

	~CaptureMonitor();

	virtual KMMEditPanel *editPanel() const;
	virtual KMMScreen *screen() const;
	virtual DocClipRef *clip() const;

	public slots: void slotSetupScreen();

	void slotRewind();
	void slotReverse();
	void slotReverseSlow();
	void slotStop();
	void slotPlaySlow();
	void slotPlay();
	void slotForward();

      private:
	 QHBox * m_mainLayout;
	QVBox *m_saveLayout;
	QVBox *m_rightLayout;
	KURLRequester *m_saveBrowser;
	KListView *m_playListView;
	KMMScreen *m_screen;

	QHBox *m_buttonLayout;

	QToolButton *m_rewindButton;
	QToolButton *m_reverseButton;
	QToolButton *m_reverseSlowButton;
	QToolButton *m_stopButton;
	QToolButton *m_playSlowButton;
	QToolButton *m_playButton;
	QToolButton *m_forwardButton;
	QToolButton *m_recordButton;
	KLed *m_renderStatus;
    };

}
#endif
