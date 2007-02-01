/***************************************************************************
                          kdenlivesetupdlg.cpp  -  description
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

#include <qlayout.h>
#include <qlabel.h>
#include <klocale.h>
#include <kiconloader.h>
#include <kconfig.h>
#include <kurlrequester.h>
#include <kcolorbutton.h>
#include <kfontdialog.h>



#include "kdenlivesetupdlg.h"
#include "kdenlive.h"
#include "kdenlivesettings.h"


namespace Gui {

    KdenliveSetupDlg::KdenliveSetupDlg(KdenliveApp * app, QWidget * parent,
	const char *name):KConfigDialog(parent, name,
	KdenliveSettings::self()) {
	page1 = new configInterface();
	page2 = new configTimeline();
        page3 = new configMisc();
        page4 = new configTitler();
        page5 = new configProject();
        page6 = new configCapture();
	
	page3->kcfg_defaulttmpfolder->setMode(KFile::Directory);
	page4->kcfg_titlerfont->enableColumn( KFontChooser::StyleList, false);
	page5->kcfg_defaultprojectformat->insertStringList(app->videoProjectFormats);
	addPage(page1, i18n("Interface"), "looknfeel");
	addPage(page2, i18n("Timeline"), "view_details");
        addPage(page3, i18n("Misc"), "run");
        addPage(page4, i18n("Titler"), "text");
        addPage(page5, i18n("Default Project"), "filenew");
        addPage(page6, i18n("Capture"), "capture");
	
	initAudioDevices();

    } 
    
    KdenliveSetupDlg::~KdenliveSetupDlg() {}

    void KdenliveSetupDlg::initAudioDevices()
    {
	page3->audio_device->insertItem(i18n("Default"));
	KProcIO *readProcess=new KProcIO();
	*readProcess << "aplay"<<"-l";
	connect(readProcess, SIGNAL(processExited(KProcess *)), this, SLOT(slotAudioSetupFinished(KProcess *)));
	connect(readProcess, SIGNAL(readReady(KProcIO *)) ,this, SLOT(slotReadAudioDevices(KProcIO *)));
	if (!readProcess->start(KProcess::NotifyOnExit, true)) kdDebug()<<"/// UNABLE TO PARSE AUDIO DEVICES"<<endl;
    }


    void KdenliveSetupDlg::slotReadAudioDevices(KProcIO *p)
    {
	QString data;
	int i = 1;
	m_audio_devices<<"0;default";
  	while (p->readln(data, true) != -1) {
	    if (data.startsWith("card")) {
		QString card = data.section(":", 0, 0).section(" ", -1);
		QString device = data.section(":", 1, 1).section(" ", -1);
		m_audio_devices<<QString::number(i) + ";plughw:" + card + "," + device;
		page3->audio_device->insertItem(data.section(":", -1));
		i++;
	    }
	}
    }

    void KdenliveSetupDlg::slotAudioSetupFinished(KProcess *)
    {
	page3->audio_device->setCurrentItem(KdenliveSettings::audiodevice().section(";",0,0).toInt());
    }

    QString KdenliveSetupDlg::selectedAudioDevice()
    {
	uint i = page3->audio_device->currentItem();
	return m_audio_devices[i];
    }

}				// namespace Gui
