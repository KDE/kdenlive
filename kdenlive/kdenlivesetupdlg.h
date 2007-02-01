/***************************************************************************
                          kdenlivesetupdlg.h  -  description
                             -------------------
    begin                : Sat Dec 28 2002
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

#ifndef KDENLIVESETUPDLG_H
#define KDENLIVESETUPDLG_H

#include <qwidget.h>
#include <kprocio.h>
#include <kconfigdialog.h>

#include "configinterface_ui.h"
#include "configtimeline_ui.h"
#include "configproject_ui.h"
#include "configmisc_ui.h"
#include "configtitler_ui.h"
#include "configcapture_ui.h"


namespace Gui {
    class KdenliveApp;
    class RenderSetupDlg;

/**This class handles the standard "Configure Kdenlive..." dialog box.
  *@author Jason Wood
  */

    class KdenliveSetupDlg:public KConfigDialog {
      Q_OBJECT public:
	KdenliveSetupDlg(KdenliveApp * app, QWidget * parent =
	    0, const char *name = 0);
	~KdenliveSetupDlg();

	QString selectedAudioDevice();
	configInterface *page1;
	configTimeline *page2;
        configMisc *page3;
        configProject *page5;
        configTitler *page4;
        configCapture *page6;

    private:
	QStringList m_audio_devices;
	void initAudioDevices();

    private slots:
	void slotReadAudioDevices(KProcIO *p);
	void slotAudioSetupFinished(KProcess *);
    };


}				// namespace Gui
#endif
