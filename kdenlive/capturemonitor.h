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

#include <kprocess.h>

#include <kdenlive.h>
#include <kmonitor.h>

class DocClipRef;
class KURLRequester;
class KListView;
class KLed;
class QToolButton;

namespace Gui {

    class KdenliveApp;
    class KMMRecPanel;
    class KMMScreen;

/**
A capture monitor, that allows you to capture video from various devices, as supported by the renderer.

@author Jean-Baptiste Mardelle
*/
    class CaptureMonitor:public KMonitor {
      Q_OBJECT public:
	CaptureMonitor(KdenliveApp * app, QWidget * parent =
	    0, const char *name = 0);

	~CaptureMonitor();

        virtual void exportCurrentFrame(KURL url) const;
	virtual KMMEditPanel *editPanel() const;
	virtual KMMScreen *screen() const;
	virtual DocClipRef *clip() const;

	public slots: 
	void slotSetupScreen();
	void slotRec();
	void slotRewind();
	void slotReverse();
	void slotFastForward();
	void slotStop(KProcess *p = 0);
	void slotPlay();
	void slotPause();
	void slotForward();
	void slotSetActive();
	void slotClickMonitor();

	private slots:
	void activateMonitor();
	void slotInit();
	void displayCapturedFiles();
	void checkCapture();

      private:

	KURLRequester *m_saveBrowser;
	KListView *m_playListView;
	QVBox *m_screenHolder;
	QWidget *m_screen;
	KMMRecPanel *m_recPanel;
	KProcess *captureProcess;
	KdenliveApp *m_app;
	bool hasCapturedFiles;
	QString m_tmpFolder;
	int m_fileNumber;
    };

}
#endif
